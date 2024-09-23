// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_registry_impl.h"

#include <stddef.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/key_system_capability.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/media/key_system_support_android.h"
#include "media/base/android/media_drm_bridge.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/key_system_support_win.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#endif

namespace content {

namespace {

bool MatchKeySystem(const CdmInfo& cdm_info, const std::string& key_system) {
  return cdm_info.key_system == key_system ||
         (cdm_info.supports_sub_key_systems &&
          media::IsSubKeySystemOf(key_system, cdm_info.key_system));
}

// Reports whether the software secure CDM is available.
void ReportSoftwareSecureCdmAvailableUMA(const std::string& key_system,
                                         bool available) {
  // Use GetKeySystemNameForUMA() without specifying `use_hw_secure_codecs` for
  // backward compatibility.
  base::UmaHistogramBoolean("Media.EME." +
                                media::GetKeySystemNameForUMA(key_system) +
                                ".LibraryCdmAvailable",
                            available);
}

constexpr media::VideoCodec kVideoCodecsToReportToUma[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    media::VideoCodec::kH264,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    media::VideoCodec::kHEVC,
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    media::VideoCodec::kDolbyVision,
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    media::VideoCodec::kVP9, media::VideoCodec::kAV1};

// Reports the status and capabilities of the hardware secure CDM. Only reported
// once per browser session per `key_system`.
void ReportHardwareSecureCapabilityStatusUMA(
    const std::string& key_system,
    CdmInfo::Status status,
    const media::CdmCapability* hw_secure_capability) {
  // Use a set to track whether the UMA has been reported for `key_system` to
  // make sure we only report once.
  static base::NoDestructor<std::set<std::string>> reported_key_systems;
  if (reported_key_systems->count(key_system))
    return;

  reported_key_systems->insert(key_system);

  auto uma_prefix =
      "Media.EME." +
      media::GetKeySystemNameForUMA(key_system, /*use_hw_secure_codecs=*/true);

  // Report whether hardware secure decryption is disabled and if so why.
  base::UmaHistogramEnumeration(uma_prefix + ".CdmInfoStatus", status);

  // When hardware secure decryption is enabled, report whether it is supported.
  if (status == CdmInfo::Status::kEnabled) {
    base::UmaHistogramBoolean(uma_prefix + ".Support", hw_secure_capability);
    // When supported, report whether a particular video codec is supported.
    if (hw_secure_capability) {
      const auto& video_codecs = hw_secure_capability->video_codecs;
      for (const auto& video_codec : kVideoCodecsToReportToUma) {
        bool is_supported = video_codecs.count(video_codec);
        base::UmaHistogramBoolean(
            uma_prefix + ".Support." + media::GetCodecNameForUMA(video_codec),
            is_supported);

        // When the codec is supported for hardware security, report whether
        // clear lead is supported or not.
        if (is_supported) {
          bool is_clear_lead_supported =
              video_codecs.at(video_codec).supports_clear_lead;
          base::UmaHistogramBoolean(uma_prefix + ".ClearLeadSupport." +
                                        media::GetCodecNameForUMA(video_codec),
                                    is_clear_lead_supported);
        }
      }
    }
  }
}

bool IsEnabled(CdmInfo::Status status) {
  return status == CdmInfo::Status::kEnabled ||
         status == CdmInfo::Status::kCommandLineOverridden;
}

// Returns a CdmCapability with codecs specified on command line. Returns null
// if kOverrideHardwareSecureCodecsForTesting was not specified or not valid
// codecs specified.
std::optional<media::CdmCapability>
GetHardwareSecureCapabilityOverriddenFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line || !command_line->HasSwitch(
                           switches::kOverrideHardwareSecureCodecsForTesting)) {
    return std::nullopt;
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
  const media::VideoCodecInfo kAllProfiles;
  const media::VideoCodecInfo kAllProfilesNoClearLead = {{}, false};
  for (const auto& codec : overridden_codecs) {
    if (codec == "vp8")
      video_codecs.emplace(media::VideoCodec::kVP8, kAllProfiles);
    else if (codec == "vp9")
      video_codecs.emplace(media::VideoCodec::kVP9, kAllProfiles);
    else if (codec == "avc1")
      video_codecs.emplace(media::VideoCodec::kH264, kAllProfiles);
    else if (codec == "hevc")
      video_codecs.emplace(media::VideoCodec::kHEVC, kAllProfiles);
    else if (codec == "dolbyvision")
      video_codecs.emplace(media::VideoCodec::kDolbyVision, kAllProfiles);
    else if (codec == "av01")
      video_codecs.emplace(media::VideoCodec::kAV1, kAllProfiles);
    else if (codec == "vp8-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kVP8, kAllProfilesNoClearLead);
    else if (codec == "vp9-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kVP9, kAllProfilesNoClearLead);
    else if (codec == "avc1-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kH264, kAllProfilesNoClearLead);
    else if (codec == "hevc-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kHEVC, kAllProfilesNoClearLead);
    else if (codec == "dolbyvision-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kDolbyVision,
                           kAllProfilesNoClearLead);
    else if (codec == "av01-no-clearlead")
      video_codecs.emplace(media::VideoCodec::kAV1, kAllProfilesNoClearLead);
    else if (codec == "mp4a")
      audio_codecs.push_back(media::AudioCodec::kAAC);
    else if (codec == "vorbis")
      audio_codecs.push_back(media::AudioCodec::kVorbis);
    else
      DVLOG(1) << "Unsupported codec specified on command line: " << codec;
  }

  if (video_codecs.empty()) {
    DVLOG(1) << "No codec codec specified on command line";
    return std::nullopt;
  }

  // Overridden codecs assume CENC and temporary session support.
  // The EncryptedMediaSupportedTypesWidevineHwSecureTest tests depend
  // on 'cbcs' not being supported.
  return media::CdmCapability(std::move(audio_codecs), std::move(video_codecs),
                              {media::EncryptionScheme::kCenc},
                              {media::CdmSessionType::kTemporary});
}

#if BUILDFLAG(IS_WIN)
bool IsMediaFoundationHardwareSecurityDisabledByGpuFeature() {
  auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager->IsGpuFeatureInfoAvailable());
  return gpu_data_manager->GetGpuFeatureInfo().IsWorkaroundEnabled(
      gpu::DISABLE_MEDIA_FOUNDATION_HARDWARE_SECURITY);
}

bool IsGpuHardwareCompositionDisabled() {
  auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  return gpu_data_manager->IsGpuCompositingDisabled() ||
         !gpu_data_manager->GetGPUInfo().overlay_info.direct_composition;
}

bool IsGpuSoftwareEmulated() {
  auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  const bool is_gpu_software_emulated =
      gpu_data_manager->GetGPUInfo().active_gpu().IsSoftwareRenderer();
  return is_gpu_software_emulated;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// static
CdmRegistry* CdmRegistry::GetInstance() {
  return CdmRegistryImpl::GetInstance();
}

// static
CdmRegistryImpl* CdmRegistryImpl::GetInstance() {
  static CdmRegistryImpl* registry = new CdmRegistryImpl();
  return registry;
}

CdmRegistryImpl::CdmRegistryImpl() {
#if BUILDFLAG(IS_WIN)
  GpuDataManagerImpl::GetInstance()->AddObserver(this);
#endif  // BUILDFLAG(IS_WIN)
}

CdmRegistryImpl::~CdmRegistryImpl() {
#if BUILDFLAG(IS_WIN)
  GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
#endif  // BUILDFLAG(IS_WIN)
}

void CdmRegistryImpl::Init() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Let embedders register CDMs.
  GetContentClient()->AddContentDecryptionModules(&cdms_, nullptr);
}

void CdmRegistryImpl::RegisterCdm(const CdmInfo& info) {
  DVLOG(1) << __func__ << ": key_system=" << info.key_system
           << ", robustness=" << info.robustness
           << ", status=" << static_cast<int>(info.status);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Always register new CDMs at the end of the list, so that the behavior is
  // consistent across the browser process's lifetime. For example, we'll always
  // use the same registered CDM for a given key system. This also means that
  // some later registered CDMs (component updated) will not be used until
  // browser restart, which is fine in most cases.
  cdms_.push_back(info);

  // Reset cached `key_system_capabilities_` to avoid notifying new observers
  // with the old capabilities and then update them again with new ones.
  // This could cause notifying observers with the same capabilities multiple
  // times, which is okay.
  key_system_capabilities_.reset();

  // If there are `key_system_capabilities_update_callbacks_` registered,
  // finalize key system capabilities and notify the callbacks. Otherwise  we'll
  // finalize key system capabilities in `ObserveKeySystemCapabilities()`.
  if (!key_system_capabilities_update_callbacks_.empty())
    FinalizeKeySystemCapabilities();
}

void CdmRegistryImpl::SetHardwareSecureCdmStatus(CdmInfo::Status status) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(status != CdmInfo::Status::kUninitialized &&
         status != CdmInfo::Status::kEnabled &&
         status != CdmInfo::Status::kCommandLineOverridden);

  bool updated = false;
  for (auto& cdm_info : cdms_) {
    if (cdm_info.robustness == CdmInfo::Robustness::kHardwareSecure) {
      cdm_info.status = status;
      updated = true;
    }
  }

  if (!updated) {
    DVLOG(1) << "No hardware secure CDMs to update";
    return;
  }

  key_system_capabilities_.reset();

  // If there are `key_system_capabilities_update_callbacks_` registered,
  // finalize key system capabilities and notify the callbacks. Otherwise  we'll
  // finalize key system capabilities in `ObserveKeySystemCapabilities()`.
  if (!key_system_capabilities_update_callbacks_.empty()) {
    FinalizeKeySystemCapabilities();
  }
}

void CdmRegistryImpl::OnGpuInfoUpdate() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  if (IsGpuHardwareCompositionDisabled())
    SetHardwareSecureCdmStatus(CdmInfo::Status::kGpuCompositionDisabled);
#endif  // BUILDFLAG(IS_WIN)
}

const std::vector<CdmInfo>& CdmRegistryImpl::GetRegisteredCdms() const {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cdms_;
}

std::unique_ptr<CdmInfo> CdmRegistryImpl::GetCdmInfo(
    const std::string& key_system,
    CdmInfo::Robustness robustness) const {
  DVLOG(2) << __func__ << ": key_system=" << key_system
           << ", robustness=" << robustness;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& cdm : cdms_) {
    if (cdm.robustness == robustness && MatchKeySystem(cdm, key_system))
      return std::make_unique<CdmInfo>(cdm);
  }

  return nullptr;
}

base::CallbackListSubscription CdmRegistryImpl::ObserveKeySystemCapabilities(
    bool allow_hw_secure_capability_check,
    KeySystemCapabilitiesUpdateCB cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto subscription = key_system_capabilities_update_callbacks_.Add(cb);

  // Re-trigger Hardware secure capability check when we encounter the first
  // observer that allows the check.
  if (allow_hw_secure_capability_check && !allow_hw_secure_capability_check_) {
    allow_hw_secure_capability_check_ = true;
    key_system_capabilities_.reset();
    pending_lazy_initializations_.clear();
    weak_ptr_factory_.InvalidateWeakPtrs();
    FinalizeKeySystemCapabilities();
    return subscription;
  }

  if (!pending_lazy_initializations_.empty()) {
    // Lazy initializing some key systems. All callbacks will be notified when
    // that's finished.
    return subscription;
  }

  if (key_system_capabilities_.has_value()) {
    cb.Run(key_system_capabilities_.value());
    return subscription;
  }

  FinalizeKeySystemCapabilities();
  return subscription;
}

std::pair<std::optional<media::CdmCapability>, CdmInfo::Status>
CdmRegistryImpl::GetCapability(const std::string& key_system,
                               CdmInfo::Robustness robustness) {
  DVLOG(2) << __func__ << ": key_system=" << key_system
           << ", robustness=" << robustness;
  using Status = CdmInfo::Status;

  if (robustness == CdmInfo::Robustness::kHardwareSecure) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLacrosUseChromeosProtectedMedia)) {
      return {std::nullopt, Status::kHardwareSecureDecryptionDisabled};
    }
#elif !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    if (!media::IsHardwareSecureDecryptionEnabled()) {
      DVLOG(1) << "Hardware secure decryption disabled";
      return {std::nullopt, Status::kHardwareSecureDecryptionDisabled};
    }
#endif  // !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

    // Secure codecs override takes precedence over other checks.
    auto overridden_capability =
        GetHardwareSecureCapabilityOverriddenFromCommandLine();
    if (overridden_capability) {
      DVLOG(1) << "Hardware secure codecs overridden from command line";
      return {overridden_capability, Status::kCommandLineOverridden};
    }

    // Hardware secure video codecs need hardware video decoder support.
    // TODO(xhwang): Make sure this check is as close as possible to the check
    // in the render process. For example, also check check GPU features like
    // GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line &&
        command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
      DVLOG(1) << "Hardware security not supported because accelerated video "
                  "decode disabled";
      return {std::nullopt, Status::kAcceleratedVideoDecodeDisabled};
    }

#if BUILDFLAG(IS_WIN)
    // Check if the GPU is disabled from gpu/config/gpu_driver_bug_list.json and
    // if the enable faulty GPU flag is disabled. If both are disabled, HW
    // security should not be supported.
    if (IsMediaFoundationHardwareSecurityDisabledByGpuFeature() &&
        !base::FeatureList::IsEnabled(
            media::kEnableFaultyGPUForMediaFoundation)) {
      DVLOG(1) << "Hardware security not supported: GPU workarounds";
      return {std::nullopt, Status::kGpuFeatureDisabled};
    }

    if (IsGpuHardwareCompositionDisabled()) {
      DVLOG(1) << "Hardware security not supported: GPU composition disabled";
      return {std::nullopt, Status::kGpuCompositionDisabled};
    }

    // Due to the bugs (crbug.com/41496376 and crbug.com/41497095),
    // `disable_media_foundation_hardware_security` workaround flag cannot be
    // enabled for the vendor ID 0x0000 and 0x1414. All software emulated GPUs
    // are considered as disabled for the media foundation hardware security.
    if (IsGpuSoftwareEmulated()) {
      DVLOG(1)
          << "Hardware security not supported: software emulated GPU enabled";
      return {std::nullopt, Status::kDisabledBySoftwareEmulatedGpu};
    }
#endif  // BUILDFLAG(IS_WIN)
  }

  auto cdm_info = GetCdmInfo(key_system, robustness);
  if (!cdm_info) {
    DVLOG(1) << "No " << robustness << " decryption CDM registered for "
             << key_system;
    return {std::nullopt, Status::kEnabled};
  }

  DCHECK(!(cdm_info->status == Status::kUninitialized && cdm_info->capability))
      << "Capability for " << robustness << " " << key_system
      << " should not have value if uninitialized.";

  return {cdm_info->capability, cdm_info->status};
}

std::pair<std::optional<media::CdmCapability>, CdmInfo::Status>
CdmRegistryImpl::GetFinalCapability(const std::string& key_system,
                                    CdmInfo::Robustness robustness) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `status` could be kUninitialized if HW secure capability checking is not
  // allowed.
  const auto [capability, status] = GetCapability(key_system, robustness);

  return {IsEnabled(status) ? capability : std::nullopt, status};
}

void CdmRegistryImpl::FinalizeKeySystemCapabilities() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key_system_capabilities_.has_value());

  // Abort existing pending LazyInitializeHardwareSecureCapability() operations
  // to avoid updating the observer twice.
  pending_lazy_initializations_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Get the set of supported key systems in case two CDMs are registered with
  // the same key system and robustness; this also avoids updating `cdms_`
  // while iterating through it.
  std::set<std::string> supported_key_systems = GetSupportedKeySystems();

  // Attempt to finalize capabilities for all key systems.
  for (const auto& key_system : supported_key_systems) {
    for (const auto robustness : {CdmInfo::Robustness::kSoftwareSecure,
                                  CdmInfo::Robustness::kHardwareSecure}) {
      AttemptToFinalizeKeySystemCapability(key_system, robustness);
    }
  }

  // If not empty, we'll handle it in OnCapabilityInitialized().
  if (pending_lazy_initializations_.empty())
    UpdateAndNotifyKeySystemCapabilities();
}

void CdmRegistryImpl::AttemptToFinalizeKeySystemCapability(
    const std::string& key_system,
    CdmInfo::Robustness robustness) {
  auto cdm_info = GetCdmInfo(key_system, robustness);
  if (!cdm_info) {
    DVLOG(1) << "No " << robustness << " CDM registered for " << key_system;
    return;
  }

  if (cdm_info->status != CdmInfo::Status::kUninitialized) {
    DVLOG(1) << robustness << " capability already finalized for "
             << key_system;
    return;
  }

  if (robustness == CdmInfo::Robustness::kHardwareSecure &&
      !allow_hw_secure_capability_check_) {
    DVLOG(1) << robustness
             << " Not allowed to get hardware secure capability for "
             << key_system;
    return;
  }

  const auto [capability, status] = GetCapability(key_system, robustness);
  if (status != CdmInfo::Status::kUninitialized) {
    FinalizeCapability(key_system, robustness, capability, status);
    return;
  }

  // Needs lazy initialize. Use base::BindPostTaskToCurrentDefault() to force a
  // post.
  pending_lazy_initializations_.insert({key_system, robustness});
  LazyInitializeCapability(
      key_system, robustness,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &CdmRegistryImpl::OnCapabilityInitialized,
          weak_ptr_factory_.GetWeakPtr(), key_system, robustness)));
}

// TODO(xhwang): Find a way to register this as callbacks so we don't have to
// hardcode platform-specific logic here.
void CdmRegistryImpl::LazyInitializeCapability(
    const std::string& key_system,
    CdmInfo::Robustness robustness,
    media::CdmCapabilityCB cdm_capability_cb) {
  DVLOG(2) << __func__ << ": key_system=" << key_system
           << ", robustness=" << robustness;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capability_cb_for_testing_) {
    capability_cb_for_testing_.Run(key_system, robustness,
                                   std::move(cdm_capability_cb));
    return;
  }

#if BUILDFLAG(IS_WIN)
  if (robustness == CdmInfo::Robustness::kHardwareSecure) {
    auto cdm_info =
        GetCdmInfo(key_system, CdmInfo::Robustness::kHardwareSecure);
    DCHECK(cdm_info && !cdm_info->capability);
    GetMediaFoundationServiceCdmCapability(key_system, cdm_info->path,
                                           /*is_hw_secure=*/true,
                                           std::move(cdm_capability_cb));
  } else {
    // kSoftwareSecure should have been determined from the manifest.
    std::move(cdm_capability_cb).Run(std::nullopt);
  }
#elif BUILDFLAG(IS_ANDROID)
  GetAndroidCdmCapability(key_system, robustness, std::move(cdm_capability_cb));
#else
  std::move(cdm_capability_cb).Run(std::nullopt);
#endif
}

void CdmRegistryImpl::OnCapabilityInitialized(
    const std::string& key_system,
    const CdmInfo::Robustness robustness,
    std::optional<media::CdmCapability> cdm_capability) {
  DVLOG(1) << __func__ << ": key_system=" << key_system
           << ", robustness=" << robustness
           << ", cdm_capability=" << (cdm_capability ? "yes" : "no");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_lazy_initializations_.count({key_system, robustness}));

  FinalizeCapability(key_system, robustness, std::move(cdm_capability),
                     CdmInfo::Status::kEnabled);

  pending_lazy_initializations_.erase({key_system, robustness});
  if (pending_lazy_initializations_.empty())
    UpdateAndNotifyKeySystemCapabilities();
}

void CdmRegistryImpl::FinalizeCapability(
    const std::string& key_system,
    const CdmInfo::Robustness robustness,
    std::optional<media::CdmCapability> cdm_capability,
    CdmInfo::Status status) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(status != CdmInfo::Status::kUninitialized);

  auto itr = cdms_.begin();
  for (; itr != cdms_.end(); itr++) {
    if (itr->robustness == robustness && MatchKeySystem(*itr, key_system)) {
      break;
    }
  }

  if (itr == cdms_.end()) {
    DLOG(ERROR) << __func__ << ": Cannot find CdmInfo to finalize for "
                << key_system << " with robustness " << robustness;
    return;
  }

  if (itr->status != CdmInfo::Status::kUninitialized) {
    DLOG(ERROR) << __func__ << ": CdmCapability already finalized for "
                << key_system << " with robustness " << robustness;
    return;
  }

  itr->status = status;
  itr->capability = cdm_capability;
#if BUILDFLAG(IS_ANDROID)
  // Querying for the CDM version requires creating a MediaDrm object, so
  // delaying it until the capability is determined.
  // TODO(crbug.com/40280540): Once querying capabilities on Android is done in
  // a separate process, include the version with the capabilities returned.
  itr->version = media::MediaDrmBridge::GetVersion(key_system);
#endif
}

void CdmRegistryImpl::UpdateAndNotifyKeySystemCapabilities() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_lazy_initializations_.empty());

  auto key_system_capabilities = GetKeySystemCapabilities();

  if (key_system_capabilities_.has_value() &&
      key_system_capabilities_.value() == key_system_capabilities) {
    DVLOG(3) << "Same key system capabilities; no need to update";
    return;
  }

  key_system_capabilities_ = key_system_capabilities;
  key_system_capabilities_update_callbacks_.Notify(key_system_capabilities);
}

std::set<std::string> CdmRegistryImpl::GetSupportedKeySystems() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<std::string> supported_key_systems;
  for (const auto& cdm : cdms_)
    supported_key_systems.insert(cdm.key_system);

  return supported_key_systems;
}

KeySystemCapabilities CdmRegistryImpl::GetKeySystemCapabilities() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  KeySystemCapabilities key_system_capabilities;

  std::set<std::string> supported_key_systems = GetSupportedKeySystems();
  for (const auto& key_system : supported_key_systems) {
    CdmInfo::Status status;
    media::KeySystemCapability capability;

    // Software secure capability.
    std::tie(capability.sw_secure_capability, status) =
        GetFinalCapability(key_system, CdmInfo::Robustness::kSoftwareSecure);
    ReportSoftwareSecureCdmAvailableUMA(
        key_system, capability.sw_secure_capability != std::nullopt);

    // Hardware secure capability.
    std::tie(capability.hw_secure_capability, status) =
        GetFinalCapability(key_system, CdmInfo::Robustness::kHardwareSecure);
    ReportHardwareSecureCapabilityStatusUMA(
        key_system, status,
        base::OptionalToPtr(capability.hw_secure_capability));

    if (capability.sw_secure_capability || capability.hw_secure_capability)
      key_system_capabilities[key_system] = std::move(capability);
  }

  return key_system_capabilities;
}

void CdmRegistryImpl::SetCapabilityCBForTesting(CapabilityCB cb) {
  capability_cb_for_testing_ = std::move(cb);
}

}  // namespace content
