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
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"

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
        base::UmaHistogramBoolean(
            uma_prefix + ".Support." + media::GetCodecNameForUMA(video_codec),
            video_codecs.count(video_codec));
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
    const CdmRegistryImpl& cdm_registry_impl,
    const std::string& key_system) {
  auto cdm_info = cdm_registry_impl.GetCdmInfo(
      key_system, CdmInfo::Robustness::kSoftwareSecure);
  if (!cdm_info) {
    ReportSoftwareSecureCdmAvailableUMA(key_system, false);
    return absl::nullopt;
  }

  ReportSoftwareSecureCdmAvailableUMA(key_system, true);

  if (!cdm_info->capability) {
    DVLOG(1) << "Lazy initialization of SoftwareSecure CdmCapability not "
                "supported!";
    return absl::nullopt;
  }

  return cdm_info->capability;
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
#endif  // BUILDFLAG(IS_WIN)

// Trying to get hardware secure capability synchronously. If lazy
// initialization is needed, set `lazy_initialize` to true.
std::tuple<absl::optional<media::CdmCapability>, CdmInfo::Status>
GetHardwareSecureCapability(const CdmRegistryImpl& cdm_registry_impl,
                            const std::string& key_system) {
  using Status = CdmInfo::Status;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosUseChromeosProtectedMedia)) {
    return {absl::nullopt, Status::kHardwareSecureDecryptionDisabled};
  }
#elif !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (!media::IsHardwareSecureDecryptionEnabled()) {
    DVLOG(1) << "Hardware secure decryption disabled";
    return {absl::nullopt, Status::kHardwareSecureDecryptionDisabled};
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
    return {absl::nullopt, Status::kAcceleratedVideoDecodeDisabled};
  }

#if BUILDFLAG(IS_WIN)
  if (IsMediaFoundationHardwareSecurityDisabledByGpuFeature()) {
    DVLOG(1) << "Hardware security not supported: GPU workarounds";
    return {absl::nullopt, Status::kGpuFeatureDisabled};
  }

  if (IsGpuHardwareCompositionDisabled()) {
    DVLOG(1) << "Hardware security not supported: GPU composition disabled";
    return {absl::nullopt, Status::kGpuCompositionDisabled};
  }
#endif  // BUILDFLAG(IS_WIN)

  auto cdm_info = cdm_registry_impl.GetCdmInfo(
      key_system, CdmInfo::Robustness::kHardwareSecure);
  if (!cdm_info) {
    DVLOG(1) << "No Hardware secure decryption CDM registered";
    return {absl::nullopt, Status::kEnabled};
  }

  DCHECK(!(cdm_info->status == CdmInfo::Status::kUninitialized &&
           cdm_info->capability))
      << "Capability should not have value if uninitialized.";

  return {cdm_info->capability, cdm_info->status};
}

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
           << ", robustness=" << static_cast<int>(info.robustness)
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
  if (!key_system_capabilities_update_callbacks_.empty())
    FinalizeKeySystemCapabilities();
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
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& cdm : cdms_) {
    if (cdm.robustness == robustness && MatchKeySystem(cdm, key_system))
      return std::make_unique<CdmInfo>(cdm);
  }

  return nullptr;
}

void CdmRegistryImpl::ObserveKeySystemCapabilities(
    KeySystemCapabilitiesUpdateCB cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  key_system_capabilities_update_callbacks_.AddUnsafe(cb);

  if (!pending_lazy_initialize_key_systems_.empty()) {
    // Lazy initializing some key systems. All callbacks will be notified when
    // that's finished.
    return;
  }

  if (key_system_capabilities_.has_value()) {
    cb.Run(key_system_capabilities_.value());
    return;
  }

  FinalizeKeySystemCapabilities();
}

void CdmRegistryImpl::FinalizeKeySystemCapabilities() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key_system_capabilities_.has_value());

  // Abort existing pending LazyInitializeHardwareSecureCapability() operations
  // to avoid updating the observer twice.
  pending_lazy_initialize_key_systems_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Get the set of supported key systems in case two CDMs are registered with
  // the same key system and robustness; this also avoids updating `cdms_`
  // while iterating through it.
  std::set<std::string> supported_key_systems = GetSupportedKeySystems();

  // Finalize hardware secure capabilities for all key systems. (Assumes
  // software secure capabilities are always already finalized.)
  for (const auto& key_system : supported_key_systems) {
    auto cdm_info =
        GetCdmInfo(key_system, CdmInfo::Robustness::kHardwareSecure);
    if (!cdm_info) {
      DVLOG(1) << "No Hardware secure CDM registered";
      continue;
    }

    if (cdm_info->status != CdmInfo::Status::kUninitialized) {
      DVLOG(1) << "Hardware secure capability already finalized";
      continue;
    }

    absl::optional<media::CdmCapability> hw_secure_capability;
    CdmInfo::Status status;
    std::tie(hw_secure_capability, status) =
        GetHardwareSecureCapability(*this, key_system);
    if (status != CdmInfo::Status::kUninitialized) {
      FinalizeHardwareSecureCapability(key_system, hw_secure_capability,
                                       status);
      continue;
    }

    // Needs lazy initialize. Use BindToCurrentLoop() to force a post.
    pending_lazy_initialize_key_systems_.insert(key_system);
    LazyInitializeHardwareSecureCapability(
        key_system, media::BindToCurrentLoop(base::BindOnce(
                        &CdmRegistryImpl::OnHardwareSecureCapabilityInitialized,
                        weak_ptr_factory_.GetWeakPtr(), key_system)));
  }

  // If not empty, we'll handle it in OnHardwareSecureCapabilityInitialized().
  if (pending_lazy_initialize_key_systems_.empty())
    UpdateAndNotifyKeySystemCapabilities();
}

// TODO(xhwang): Find a way to register this as callbacks so we don't have to
// hardcode platform-specific logic here.
// TODO(jrummell): Support Android query.
void CdmRegistryImpl::LazyInitializeHardwareSecureCapability(
    const std::string& key_system,
    CdmCapabilityCB cdm_capability_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hw_secure_capability_cb_for_testing_) {
    hw_secure_capability_cb_for_testing_.Run(key_system,
                                             std::move(cdm_capability_cb));
    return;
  }

#if BUILDFLAG(IS_WIN)
  auto cdm_info = GetCdmInfo(key_system, CdmInfo::Robustness::kHardwareSecure);
  DCHECK(cdm_info && !cdm_info->capability);
  GetMediaFoundationServiceHardwareSecureCdmCapability(
      key_system, cdm_info->path, std::move(cdm_capability_cb));
#else
  std::move(cdm_capability_cb).Run(absl::nullopt);
#endif  // BUILDFLAG(IS_WIN)
}

void CdmRegistryImpl::OnHardwareSecureCapabilityInitialized(
    const std::string& key_system,
    absl::optional<media::CdmCapability> cdm_capability) {
  DVLOG(1) << __func__ << ": key_system=" << key_system
           << ", cdm_capability=" << (cdm_capability ? "yes" : "no");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_lazy_initialize_key_systems_.count(key_system));

  FinalizeHardwareSecureCapability(key_system, std::move(cdm_capability),
                                   CdmInfo::Status::kEnabled);

  pending_lazy_initialize_key_systems_.erase(key_system);
  if (pending_lazy_initialize_key_systems_.empty())
    UpdateAndNotifyKeySystemCapabilities();
}

void CdmRegistryImpl::FinalizeHardwareSecureCapability(
    const std::string& key_system,
    absl::optional<media::CdmCapability> cdm_capability,
    CdmInfo::Status status) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(status != CdmInfo::Status::kUninitialized);

  auto itr = cdms_.begin();
  for (; itr != cdms_.end(); itr++) {
    if (itr->robustness == CdmInfo::Robustness::kHardwareSecure &&
        MatchKeySystem(*itr, key_system)) {
      break;
    }
  }

  if (itr == cdms_.end()) {
    DLOG(ERROR) << __func__ << ": Cannot find CdmInfo to finalize";
    return;
  }

  if (itr->status != CdmInfo::Status::kUninitialized) {
    DLOG(ERROR) << __func__ << ": CdmCapability already finalized";
    return;
  }

  itr->status = status;
  itr->capability = cdm_capability;
}

void CdmRegistryImpl::UpdateAndNotifyKeySystemCapabilities() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    media::mojom::KeySystemCapability capability;

    // Software secure capability
    capability.sw_secure_capability =
        GetSoftwareSecureCapability(*this, key_system);

    // Hardware secure capability
    absl::optional<media::CdmCapability> hw_secure_capability;
    CdmInfo::Status status;
    std::tie(hw_secure_capability, status) =
        GetHardwareSecureCapability(*this, key_system);
    DCHECK(status != CdmInfo::Status::kUninitialized);
    ReportHardwareSecureCapabilityStatusUMA(
        key_system, status, base::OptionalToPtr(hw_secure_capability));
    capability.hw_secure_capability =
        IsEnabled(status) ? hw_secure_capability : absl::nullopt;

    if (capability.sw_secure_capability || capability.hw_secure_capability)
      key_system_capabilities[key_system] = std::move(capability);
  }

  return key_system_capabilities;
}

void CdmRegistryImpl::SetHardwareSecureCapabilityCBForTesting(
    HardwareSecureCapabilityCB cb) {
  hw_secure_capability_cb_for_testing_ = std::move(cb);
}

}  // namespace content
