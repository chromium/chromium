// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_WIN)
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/key_system_support_win.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#endif

namespace content {

namespace {

void SendCdmAvailableUMA(const std::string& key_system, bool available) {
  base::UmaHistogramBoolean("Media.EME." +
                                media::GetKeySystemNameForUMA(key_system) +
                                ".LibraryCdmAvailable",
                            available);
}

// Returns a CdmCapability with codecs specified on command line. Returns null
// if kOverrideHardwareSecureCodecsForTesting was not specified or not valid
// codecs specified.
absl::optional<media::CdmCapability>
GetHardwareSecureCapabilityOverriddenFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line || !command_line->HasSwitch(
                           switches::kOverrideHardwareSecureCodecsForTesting)) {
    return absl::nullopt;
  }

  auto overridden_codecs_string = command_line->GetSwitchValueASCII(
      switches::kOverrideHardwareSecureCodecsForTesting);
  const auto overridden_codecs =
      base::SplitStringPiece(overridden_codecs_string, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // As the command line switch does not include profiles, specify {} to
  // indicate that all relevant profiles should be considered supported.
  std::vector<media::AudioCodec> audio_codecs;
  media::CdmCapability::VideoCodecMap video_codecs;
  for (const auto& codec : overridden_codecs) {
    if (codec == "vp8")
      video_codecs[media::VideoCodec::kVP8] = {};
    else if (codec == "vp9")
      video_codecs[media::VideoCodec::kVP9] = {};
    else if (codec == "avc1")
      video_codecs[media::VideoCodec::kH264] = {};
    else if (codec == "hevc")
      video_codecs[media::VideoCodec::kHEVC] = {};
    else if (codec == "mp4a")
      audio_codecs.push_back(media::AudioCodec::kAAC);
    else if (codec == "vorbis")
      audio_codecs.push_back(media::AudioCodec::kVorbis);
    else
      DVLOG(1) << "Unsupported codec specified on command line: " << codec;
  }

  if (video_codecs.empty()) {
    DVLOG(1) << "No codec codec specified on command line";
    return absl::nullopt;
  }

  // Overridden codecs assume CENC and temporary session support.
  // The EncryptedMediaSupportedTypesWidevineHwSecureTest tests depend
  // on 'cbcs' not being supported.
  return media::CdmCapability(std::move(audio_codecs), std::move(video_codecs),
                              {media::EncryptionScheme::kCenc},
                              {media::CdmSessionType::kTemporary});
}

// Software secure capability can be obtained synchronously in all supported
// cases. If needed, this can be easily converted to an asynchronous call.
absl::optional<media::CdmCapability> GetSoftwareSecureCapability(
    const std::string& key_system) {
  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kSoftwareSecure);
  if (!cdm_info) {
    SendCdmAvailableUMA(key_system, false);
    return absl::nullopt;
  }

  SendCdmAvailableUMA(key_system, true);

  if (!cdm_info->capability) {
    DVLOG(1) << "Lazy initialization of SoftwareSecure CdmCapability not "
                "supported!";
    return absl::nullopt;
  }

  return cdm_info->capability;
}

// Trying to get hardware secure capability synchronously. If lazy
// initialization is needed, set `lazy_initialize` to true.
absl::optional<media::CdmCapability> GetHardwareSecureCapability(
    const std::string& key_system,
    bool* lazy_initialize) {
  *lazy_initialize = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedMedia)) {
    return absl::nullopt;
  }
#elif !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (!base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    DVLOG(1) << "Hardware secure decryption disabled";
    return absl::nullopt;
  }
#endif  // !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

  // Secure codecs override takes precedence over other checks.
  auto overridden_capability =
      GetHardwareSecureCapabilityOverriddenFromCommandLine();
  if (overridden_capability) {
    DVLOG(1) << "Hardware secure codecs overridden from command line";
    return overridden_capability;
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
    return absl::nullopt;
  }

#if defined(OS_WIN)
  DCHECK(GpuDataManagerImpl::GetInstance()->IsGpuFeatureInfoAvailable());
  if (GpuDataManagerImpl::GetInstance()
          ->GetGpuFeatureInfo()
          .IsWorkaroundEnabled(
              gpu::DISABLE_MEDIA_FOUNDATION_HARDWARE_SECURITY)) {
    DVLOG(1) << "Disable Media Foundation Hardware security due to GPU "
                "workarounds";

    return absl::nullopt;
  }
#endif  // defined(OS_WIN)

  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kHardwareSecure);
  if (!cdm_info) {
    DVLOG(1) << "No Hardware secure decryption CDM registered";
    return absl::nullopt;
  }

  if (cdm_info->capability) {
    DVLOG(1) << "Hardware secure decryption CDM registered";
    return cdm_info->capability;
  }

  DVLOG(1) << "Lazy initialization of CdmCapability";
  *lazy_initialize = true;
  return absl::nullopt;
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
  DVLOG(3) << __func__ << ": key_system=" << key_system;

  bool lazy_initialize = false;
  auto hw_secure_capability =
      GetHardwareSecureCapability(key_system, &lazy_initialize);

  if (lazy_initialize) {
    LazyInitializeHardwareSecureCapability(
        key_system,
        base::BindOnce(&KeySystemSupportImpl::OnHardwareSecureCapability,
                       weak_ptr_factory_.GetWeakPtr(), key_system,
                       std::move(callback), /*lazy_initialize=*/true));
    return;
  }

  OnHardwareSecureCapability(key_system, std::move(callback),
                             /*lazy_initialize=*/false, hw_secure_capability);
}

// It's possible this is called multiple times for the same key system when
// there are parallel `IsKeySystemSupported()` calls from different renderer
// processes. Since the query is typically fast, the chance for this to happen
// is low and it won't cause any collision. So we choose not to handle this
// case explicitly for simplicity.
// TODO(xhwang): Find a way to register this as callbacks so we don't have to
// hardcode platform-specific logic here.
// TODO(jrummell): Support Android query.
void KeySystemSupportImpl::LazyInitializeHardwareSecureCapability(
    const std::string& key_system,
    CdmCapabilityCB cdm_capability_cb) {
  if (hw_secure_capability_cb_for_testing_) {
    hw_secure_capability_cb_for_testing_.Run(key_system,
                                             std::move(cdm_capability_cb));
    return;
  }

#if defined(OS_WIN)
  auto cdm_info = CdmRegistryImpl::GetInstance()->GetCdmInfo(
      key_system, CdmInfo::Robustness::kHardwareSecure);
  DCHECK(cdm_info && !cdm_info->capability);
  GetMediaFoundationServiceHardwareSecureCdmCapability(
      key_system, cdm_info->path, std::move(cdm_capability_cb));
#else
  std::move(cdm_capability_cb).Run(absl::nullopt);
#endif  // defined(OS_WIN)
}

void KeySystemSupportImpl::SetHardwareSecureCapabilityCBForTesting(
    HardwareSecureCapabilityCB cb) {
  hw_secure_capability_cb_for_testing_ = std::move(cb);
}

void KeySystemSupportImpl::OnHardwareSecureCapability(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback,
    bool lazy_initialize,
    absl::optional<media::CdmCapability> hw_secure_capability) {
  // See comment above. This could be called multiple times when we have
  // parallel `IsKeySystemSupported()` calls from different renderer processes.
  // This is okay and won't cause collision or corruption of data.
  if (lazy_initialize) {
    ignore_result(CdmRegistryImpl::GetInstance()->FinalizeCdmCapability(
        key_system, CdmInfo::Robustness::kHardwareSecure,
        hw_secure_capability));
  }

  auto capability = media::mojom::KeySystemCapability::New();
  capability->sw_secure_capability = GetSoftwareSecureCapability(key_system);
  capability->hw_secure_capability = hw_secure_capability;

  if (!capability->sw_secure_capability && !capability->hw_secure_capability) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  std::move(callback).Run(true, std::move(capability));
}

}  // namespace content
