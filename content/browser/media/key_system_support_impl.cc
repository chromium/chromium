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
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
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

bool GetSoftwareSecureCapabilities(
    const std::string& key_system,
    std::vector<media::VideoCodec>* video_codecs,
    std::vector<media::EncryptionScheme>* encryption_schemes,
    std::vector<media::CdmSessionType>* session_types) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kSoftwareSecure);
  if (!cdm_info) {
    SendCdmAvailableUMA(key_system, false);
    return false;
  }

  SendCdmAvailableUMA(key_system, true);

  *video_codecs = cdm_info->capability.video_codecs;
  *encryption_schemes = SetToVector(cdm_info->capability.encryption_schemes);
  *session_types = SetToVector(cdm_info->capability.session_types);
  return true;
}

bool GetHardwareSecureCapabilities(
    const std::string& key_system,
    std::vector<media::VideoCodec>* video_codecs,
    std::vector<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  // We use the USE_CHROMEOS_PROTECTED_MEDIA build flag on Chrome OS to control
  // when HW secure decryption is enabled, so disable the feature flag in that
  // case.
#if !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (!base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    DVLOG(1) << "Hardware secure decryption disabled";
    return false;
  }
#endif

  // Secure codecs override takes precedence over other checks.
  if (IsHardwareSecureCodecsOverriddenFromCommandLine(video_codecs,
                                                      encryption_schemes)) {
    DVLOG(1) << "Hardware secure codecs overridden from command line";
    return true;
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
    return false;
  }

  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kHardwareSecure);
  if (!cdm_info)
    return false;

  // TODO(xhwang): Also populate supported session types.
  *video_codecs = cdm_info->capability.video_codecs;
  *encryption_schemes = SetToVector(cdm_info->capability.encryption_schemes);
  return true;
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

KeySystemSupportImpl::KeySystemSupportImpl() = default;

KeySystemSupportImpl::~KeySystemSupportImpl() = default;

void KeySystemSupportImpl::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(3) << __func__ << ": key_system = " << key_system;

  auto capability = media::mojom::KeySystemCapability::New();
  bool sw_secure_supported = GetSoftwareSecureCapabilities(
      key_system, &capability->video_codecs, &capability->encryption_schemes,
      &capability->session_types);
  bool hw_secure_supported = GetHardwareSecureCapabilities(
      key_system, &capability->hw_secure_video_codecs,
      &capability->hw_secure_encryption_schemes);

  if (!sw_secure_supported && !hw_secure_supported) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  std::move(callback).Run(true, std::move(capability));
}

}  // namespace content
