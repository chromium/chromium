// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

void SendCdmAvailableUMA(const std::string& key_system, bool available) {
  base::UmaHistogramBoolean("Media.EME." +
                                media::GetKeySystemNameForUMA(key_system) +
                                ".LibraryCdmAvailable",
                            available);
}

template <typename T>
std::vector<T> SetToVector(const base::flat_set<T>& s) {
  return std::vector<T>(s.begin(), s.end());
}

// Returns whether hardware secure codecs are enabled from command line. If
// true, |video_codecs| are filled with codecs specified on command line, which
// could be empty if no codecs are specified. If false, |video_codecs| will not
// be modified.
bool IsHardwareSecureCodecsOverriddenFromCommandLine(
    std::vector<media::VideoCodec>* video_codecs,
    std::vector<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line || !command_line->HasSwitch(
                           switches::kOverrideHardwareSecureCodecsForTesting)) {
    return false;
  }

  auto codecs_string = command_line->GetSwitchValueASCII(
      switches::kOverrideHardwareSecureCodecsForTesting);
  const auto supported_codecs = base::SplitStringPiece(
      codecs_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& codec : supported_codecs) {
    if (codec == "vp8")
      video_codecs->push_back(media::VideoCodec::kCodecVP8);
    else if (codec == "vp9")
      video_codecs->push_back(media::VideoCodec::kCodecVP9);
    else if (codec == "avc1")
      video_codecs->push_back(media::VideoCodec::kCodecH264);
    else
      DVLOG(1) << "Unsupported codec specified on command line: " << codec;
  }

  // Codecs enabled from command line assumes CENC support.
  if (!video_codecs->empty())
    encryption_schemes->push_back(media::EncryptionScheme::kCenc);

  return true;
}

void GetHardwareSecureDecryptionCaps(
    const std::string& key_system,
    const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
    std::vector<media::VideoCodec>* video_codecs,
    std::vector<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  if (!base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    DVLOG(1) << "Hardware secure decryption disabled";
    return;
  }

  // Secure codecs override takes precedence over other checks.
  if (IsHardwareSecureCodecsOverriddenFromCommandLine(video_codecs,
                                                      encryption_schemes)) {
    DVLOG(1) << "Hardware secure codecs overridden from command line";
    return;
  }

  if (cdm_proxy_protocols.empty()) {
    DVLOG(1) << "CDM does not support any CdmProxy protocols";
    return;
  }

  // Hardware secure video codecs need hardware video decoder support.
  // TODO(xhwang): Make sure this check is as close as possible to the check
  // in the render process. For example, also check check GPU features like
  // GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
    DVLOG(1) << "Hardware secure codecs not supported because accelerated "
                "video decode disabled";
    return;
  }

#if !BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  DVLOG(1) << "Hardware secure codecs not supported because mojo video "
              "decode was disabled at buildtime";
  return;
#endif

  base::flat_set<media::VideoCodec> video_codec_set;
  base::flat_set<media::EncryptionScheme> encryption_scheme_set;

  GetContentClient()->browser()->GetHardwareSecureDecryptionCaps(
      key_system, cdm_proxy_protocols, &video_codec_set,
      &encryption_scheme_set);

  *video_codecs = SetToVector(video_codec_set);
  *encryption_schemes = SetToVector(encryption_scheme_set);
}

}  // namespace

// static
void KeySystemSupportImpl::Create(
    mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver) {
  DVLOG(3) << __func__;
  // The created object is bound to (and owned by) |request|.
  mojo::MakeSelfOwnedReceiver(std::make_unique<KeySystemSupportImpl>(),
                              std::move(receiver));
}

// static
std::unique_ptr<CdmInfo> KeySystemSupportImpl::GetCdmInfoForKeySystem(
    const std::string& key_system) {
  DVLOG(2) << __func__ << ": key_system = " << key_system;
  for (const auto& cdm : CdmRegistry::GetInstance()->GetAllRegisteredCdms()) {
    if (cdm.supported_key_system == key_system ||
        (cdm.supports_sub_key_systems &&
         media::IsChildKeySystemOf(key_system, cdm.supported_key_system))) {
      return std::make_unique<CdmInfo>(cdm);
    }
  }

  return nullptr;
}

KeySystemSupportImpl::KeySystemSupportImpl() = default;

KeySystemSupportImpl::~KeySystemSupportImpl() = default;

void KeySystemSupportImpl::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(3) << __func__ << ": key_system = " << key_system;

  auto cdm_info = GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    SendCdmAvailableUMA(key_system, false);
    std::move(callback).Run(false, nullptr);
    return;
  }

  SendCdmAvailableUMA(key_system, true);

  // Supported codecs and encryption schemes.
  auto capability = media::mojom::KeySystemCapability::New();
  capability->video_codecs = cdm_info->capability.video_codecs;
  capability->supports_vp9_profile2 =
      cdm_info->capability.supports_vp9_profile2;
  capability->encryption_schemes =
      SetToVector(cdm_info->capability.encryption_schemes);

  GetHardwareSecureDecryptionCaps(key_system,
                                  cdm_info->capability.cdm_proxy_protocols,
                                  &capability->hw_secure_video_codecs,
                                  &capability->hw_secure_encryption_schemes);

  capability->session_types = SetToVector(cdm_info->capability.session_types);

  std::move(callback).Run(true, std::move(capability));
}

}  // namespace content
