// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/web_encrypted_media_client_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "third_party/blink/public/platform/web_content_decryption_module_result.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_access_impl.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_impl.h"

namespace blink {

namespace {

// Used to name UMAs in Reporter.
const char kKeySystemSupportUMAPrefix[] =
    "Media.EME.RequestMediaKeySystemAccess.";

std::string ConvertCreateCdmStatusToString(media::CreateCdmStatus status) {
  switch (status) {
    case media::CreateCdmStatus::kSuccess:
      return "Succeeded.";
    case media::CreateCdmStatus::kUnknownError:
      return "Unknown error.";
    case media::CreateCdmStatus::kCdmCreationAborted:
      return "CDM creation aborted.";
    case media::CreateCdmStatus::kCreateCdmFuncNotAvailable:
      return "CreateCdmFunc not available.";
    case media::CreateCdmStatus::kCdmHelperCreationFailed:
      return "CDM helper creation failed.";
    case media::CreateCdmStatus::kGetCdmPrefDataFailed:
      return "Failed to get the CDM preference data.";
    case media::CreateCdmStatus::kGetCdmOriginIdFailed:
      return "Failed to get the CDM origin ID.";
    case media::CreateCdmStatus::kInitCdmFailed:
      return "Failed to initialize CDM.";
    case media::CreateCdmStatus::kCdmFactoryCreationFailed:
      return "CDM Factory creation failed.";
    case media::CreateCdmStatus::kCdmNotSupported:
      return "CDM not supported.";
    case media::CreateCdmStatus::kInvalidCdmConfig:
      return "Invalid CdmConfig.";
    case media::CreateCdmStatus::kUnsupportedKeySystem:
      return "Unsupported key system.";
    case media::CreateCdmStatus::kDisconnectionError:
      return "Disconnection error.";
    case media::CreateCdmStatus::kNotAllowedOnUniqueOrigin:
      return "EME use is not allowed on unique origins.";
#if BUILDFLAG(IS_ANDROID)
    case media::CreateCdmStatus::kMediaDrmBridgeCreationFailed:
      return "MediaDrmBridge creation failed.";
    case media::CreateCdmStatus::kMediaCryptoNotAvailable:
      return "MediaCrypto not available.";
    case media::CreateCdmStatus::kAndroidMediaDrmIllegalArgument:
      return "Illegal argument passed to MediaDrm.";
    case media::CreateCdmStatus::kAndroidMediaDrmIllegalState:
      return "MediaDrm not initialized properly.";
    case media::CreateCdmStatus::kAndroidFailedL1SecurityLevel:
      return "Unable to set L1 security level.";
    case media::CreateCdmStatus::kAndroidFailedL3SecurityLevel:
      return "Unable to set L3 security level.";
    case media::CreateCdmStatus::kAndroidFailedSecurityOrigin:
      return "Unable to set origin.";
    case media::CreateCdmStatus::kAndroidFailedMediaCryptoSession:
      return "Unable to create MediaCrypto session.";
    case media::CreateCdmStatus::kAndroidFailedToStartProvisioning:
      return "Unable to create MediaCrypto session.";
    case media::CreateCdmStatus::kAndroidFailedMediaCryptoCreate:
      return "Unable to create MediaCrypto object.";
    case media::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme:
      return "Crypto scheme not supported.";
#elif BUILDFLAG(IS_CHROMEOS)
    case media::CreateCdmStatus::kNoMoreInstances:
      return "Only one instance allowed.";
    case media::CreateCdmStatus::kInsufficientGpuResources:
      return "Insufficient GPU memory available.";
    case media::CreateCdmStatus::kCrOsVerifiedAccessDisabled:
      return "Verified Access is disabled.";
    case media::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed:
      return "Remote factory creation failed.";
#endif  // BUILDFLAG(IS_ANDROID)
    default:
      return base::ToString(status);
  }
}

// A helper function to complete WebContentDecryptionModuleResult. Used
// to convert WebContentDecryptionModuleResult to a callback.
void CompleteWebContentDecryptionModuleResult(
    std::unique_ptr<WebContentDecryptionModuleResult> result,
    std::unique_ptr<WebContentDecryptionModule> cdm,
    media::CreateCdmStatus status) {
  DCHECK(result);

  if (!cdm) {
    result->CompleteWithError(
        kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        WebString::FromASCII(ConvertCreateCdmStatusToString(status)));
    return;
  }

  result->CompleteWithContentDecryptionModule(std::move(cdm));
}

}  // namespace

struct UMAReportStatus {
  bool is_request_reported = false;
  bool is_result_reported = false;
  base::TimeTicks request_start_time;
};

// Report usage of key system to UMA. There are 2 different UMAs logged:
// 1. The resolve time of the key system.
// 2. The reject time of the key system.
// At most one of each will be reported at most once per process.
class PerProcessReporter {
 public:
  explicit PerProcessReporter(const std::string& key_system_for_uma)
      : uma_name_(kKeySystemSupportUMAPrefix + key_system_for_uma) {}
  ~PerProcessReporter() = default;

  void ReportRequested() {
    if (report_status_.is_request_reported) {
      return;
    }

    report_status_.is_request_reported = true;
    report_status_.request_start_time = base::TimeTicks::Now();
  }

  void ReportResolveTime() {
    DCHECK(report_status_.is_request_reported);
    if (report_status_.is_result_reported) {
      return;
    }

    base::UmaHistogramTimes(
        uma_name_ + ".TimeTo.Resolve",
        base::TimeTicks::Now() - report_status_.request_start_time);
    report_status_.is_result_reported = true;
  }

  void ReportRejectTime() {
    if (report_status_.is_result_reported) {
      return;
    }

    base::UmaHistogramTimes(
        uma_name_ + ".TimeTo.Reject",
        base::TimeTicks::Now() - report_status_.request_start_time);
    report_status_.is_result_reported = true;
  }

 private:
  const std::string uma_name_;
  UMAReportStatus report_status_;
};

using PerProcessReporterMap =
    std::unordered_map<std::string, std::unique_ptr<PerProcessReporter>>;

PerProcessReporterMap& GetPerProcessReporterMap() {
  static base::NoDestructor<PerProcessReporterMap> per_process_reporters_map;
  return *per_process_reporters_map;
}

static PerProcessReporter* GetPerProcessReporter(const WebString& key_system) {
  // Assumes that empty will not be found by GetKeySystemNameForUMA().
  std::string key_system_ascii;
  if (key_system.ContainsOnlyASCII()) {
    key_system_ascii = key_system.Ascii();
  }

  std::string uma_name = media::GetKeySystemNameForUMA(key_system_ascii);

  std::unique_ptr<PerProcessReporter>& reporter =
      GetPerProcessReporterMap()[uma_name];

  if (!reporter) {
    reporter = std::make_unique<PerProcessReporter>(uma_name);
  }

  return reporter.get();
}

// Report usage of key system to UMA. There are 2 different counts logged:
// 1. The key system is requested.
// 2. The requested key system and options are supported.
// Each stat is only reported once per renderer frame per key system.
// Note that WebEncryptedMediaClientImpl is only created once by each
// renderer frame.
class WebEncryptedMediaClientImpl::Reporter {
 public:
  enum KeySystemSupportStatus {
    KEY_SYSTEM_REQUESTED = 0,
    KEY_SYSTEM_SUPPORTED = 1,
    KEY_SYSTEM_SUPPORT_STATUS_COUNT
  };

  explicit Reporter(const std::string& key_system_for_uma)
      : uma_name_(kKeySystemSupportUMAPrefix + key_system_for_uma),
        is_request_reported_(false),
        is_support_reported_(false) {}
  ~Reporter() = default;

  void ReportRequested() {
    if (is_request_reported_)
      return;
    Report(KEY_SYSTEM_REQUESTED);
    is_request_reported_ = true;
  }

  void ReportSupported() {
    DCHECK(is_request_reported_);
    if (is_support_reported_)
      return;
    Report(KEY_SYSTEM_SUPPORTED);
    is_support_reported_ = true;
  }

 private:
  void Report(KeySystemSupportStatus status) {
    base::UmaHistogramEnumeration(uma_name_, status,
                                  KEY_SYSTEM_SUPPORT_STATUS_COUNT);
  }

  const std::string uma_name_;
  bool is_request_reported_;
  bool is_support_reported_;
};

WebEncryptedMediaClientImpl::WebEncryptedMediaClientImpl(
    media::KeySystems* key_systems,
    media::CdmFactory* cdm_factory,
    media::MediaPermission* media_permission,
    std::unique_ptr<KeySystemConfigSelector::WebLocalFrameDelegate>
        web_frame_delegate)
    : key_systems_(key_systems),
      cdm_factory_(cdm_factory),
      key_system_config_selector_(key_systems_,
                                  media_permission,
                                  std::move(web_frame_delegate)) {
  DCHECK(cdm_factory_);
}

WebEncryptedMediaClientImpl::~WebEncryptedMediaClientImpl() = default;

void WebEncryptedMediaClientImpl::RequestMediaKeySystemAccess(
    WebEncryptedMediaRequest request) {
  GetReporter(request.KeySystem())->ReportRequested();

  GetPerProcessReporter(request.KeySystem())->ReportRequested();

  pending_requests_.push_back(std::move(request));
  key_systems_->UpdateIfNeeded(
      base::BindOnce(&WebEncryptedMediaClientImpl::OnKeySystemsUpdated,
                     weak_factory_.GetWeakPtr()));
}

void WebEncryptedMediaClientImpl::CreateCdm(
    const WebSecurityOrigin& security_origin,
    const media::CdmConfig& cdm_config,
    std::unique_ptr<WebContentDecryptionModuleResult> result) {
  WebContentDecryptionModuleImpl::Create(
      cdm_factory_, key_systems_, security_origin, cdm_config,
      base::BindOnce(&CompleteWebContentDecryptionModuleResult,
                     std::move(result)));
}

void WebEncryptedMediaClientImpl::OnKeySystemsUpdated() {
  auto requests = std::move(pending_requests_);
  for (const auto& request : requests)
    SelectConfig(request);
}

void WebEncryptedMediaClientImpl::SelectConfig(
    WebEncryptedMediaRequest request) {
  key_system_config_selector_.SelectConfig(
      request.KeySystem(), request.SupportedConfigurations(),
      base::BindOnce(&WebEncryptedMediaClientImpl::OnConfigSelected,
                     weak_factory_.GetWeakPtr(), request));
}

void WebEncryptedMediaClientImpl::OnConfigSelected(
    WebEncryptedMediaRequest request,
    KeySystemConfigSelector::Status status,
    WebMediaKeySystemConfiguration* accumulated_configuration,
    media::CdmConfig* cdm_config) {
  // Update encrypted_media_supported_types_browsertest.cc if updating these
  // strings.
  // TODO(xhwang): Consider using different messages for kUnsupportedKeySystem
  // and kUnsupportedConfigs.
  const char kUnsupportedKeySystemOrConfigMessage[] =
      "Unsupported keySystem or supportedConfigurations.";
  // Handle unsupported cases first.
  switch (status) {
    case KeySystemConfigSelector::Status::kUnsupportedKeySystem:
    case KeySystemConfigSelector::Status::kUnsupportedConfigs:
      request.RequestNotSupported(kUnsupportedKeySystemOrConfigMessage);
      GetPerProcessReporter(request.KeySystem())->ReportRejectTime();
      return;
    case KeySystemConfigSelector::Status::kSupported:
      break;  // Handled below.
  }

  // Use the requested key system to match what's reported in
  // RequestMediaKeySystemAccess().
  DCHECK_EQ(status, KeySystemConfigSelector::Status::kSupported);
  GetReporter(request.KeySystem())->ReportSupported();
  GetPerProcessReporter(request.KeySystem())->ReportResolveTime();

  // If the frame is closed while the permission prompt is displayed,
  // the permission prompt is dismissed and this may result in the
  // requestMediaKeySystemAccess request succeeding. However, the blink
  // objects may have been cleared, so check if this is the case and simply
  // reject the request.
  WebSecurityOrigin origin = request.GetSecurityOrigin();
  if (origin.IsNull()) {
    request.RequestNotSupported("Unable to create MediaKeySystemAccess");
    return;
  }

  // Use the returned key system which should be used for CDM creation.
  request.RequestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
      origin, *accumulated_configuration, *cdm_config,
      weak_factory_.GetWeakPtr()));
}

WebEncryptedMediaClientImpl::Reporter* WebEncryptedMediaClientImpl::GetReporter(
    const WebString& key_system) {
  // Assumes that empty will not be found by GetKeySystemNameForUMA().
  std::string key_system_ascii;
  if (key_system.ContainsOnlyASCII())
    key_system_ascii = key_system.Ascii();

  // Return a per-frame singleton so that UMA reports will be once-per-frame.
  std::string uma_name = media::GetKeySystemNameForUMA(key_system_ascii);
  std::unique_ptr<Reporter>& reporter = reporters_[uma_name];
  if (!reporter)
    reporter = std::make_unique<Reporter>(uma_name);
  return reporter.get();
}
}  // namespace blink
