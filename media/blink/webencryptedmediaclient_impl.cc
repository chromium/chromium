// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webencryptedmediaclient_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webcontentdecryptionmoduleaccess_impl.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_content_decryption_module_result.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

namespace {

// Used to name UMAs in Reporter.
const char kKeySystemSupportUMAPrefix[] =
    "Media.EME.RequestMediaKeySystemAccess.";

// A helper function to complete blink::WebContentDecryptionModuleResult. Used
// to convert blink::WebContentDecryptionModuleResult to a callback.
void CompleteWebContentDecryptionModuleResult(
    std::unique_ptr<blink::WebContentDecryptionModuleResult> result,
    blink::WebContentDecryptionModule* cdm,
    const std::string& error_message) {
  DCHECK(result);

  if (!cdm) {
    result->CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        blink::WebString::FromUTF8(error_message));
    return;
  }

  result->CompleteWithContentDecryptionModule(cdm);
}

}  // namespace

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
    CdmFactory* cdm_factory,
    MediaPermission* media_permission)
    : cdm_factory_(cdm_factory),
      key_system_config_selector_(KeySystems::GetInstance(), media_permission) {
  DCHECK(cdm_factory_);
}

WebEncryptedMediaClientImpl::~WebEncryptedMediaClientImpl() = default;

void WebEncryptedMediaClientImpl::RequestMediaKeySystemAccess(
    blink::WebEncryptedMediaRequest request) {
  GetReporter(request.KeySystem())->ReportRequested();

  key_system_config_selector_.SelectConfig(
      request.KeySystem(), request.SupportedConfigurations(),
      base::Bind(&WebEncryptedMediaClientImpl::OnRequestSucceeded,
                 weak_factory_.GetWeakPtr(), request),
      base::Bind(&WebEncryptedMediaClientImpl::OnRequestNotSupported,
                 weak_factory_.GetWeakPtr(), request));
}

void WebEncryptedMediaClientImpl::CreateCdm(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    const CdmConfig& cdm_config,
    std::unique_ptr<blink::WebContentDecryptionModuleResult> result) {
  WebContentDecryptionModuleImpl::Create(
      cdm_factory_, key_system.Utf16(), security_origin, cdm_config,
      base::BindOnce(&CompleteWebContentDecryptionModuleResult,
                     std::move(result)));
}

void WebEncryptedMediaClientImpl::OnRequestSucceeded(
    blink::WebEncryptedMediaRequest request,
    const blink::WebMediaKeySystemConfiguration& accumulated_configuration,
    const CdmConfig& cdm_config) {
  GetReporter(request.KeySystem())->ReportSupported();
  // TODO(sandersd): Pass |are_secure_codecs_required| along and use it to
  // configure the CDM security level and use of secure surfaces on Android.

  // If the frame is closed while the permission prompt is displayed,
  // the permission prompt is dismissed and this may result in the
  // requestMediaKeySystemAccess request succeeding. However, the blink
  // objects may have been cleared, so check if this is the case and simply
  // reject the request.
  blink::WebSecurityOrigin origin = request.GetSecurityOrigin();
  if (origin.IsNull()) {
    request.RequestNotSupported("Unable to create MediaKeySystemAccess");
    return;
  }

  request.RequestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
      request.KeySystem(), origin, accumulated_configuration, cdm_config,
      weak_factory_.GetWeakPtr()));
}

void WebEncryptedMediaClientImpl::OnRequestNotSupported(
    blink::WebEncryptedMediaRequest request) {
  // The rejection message when the key system is not supported or when none of
  // the requested configurations is supported should always be the same to help
  // avoid leaking information unnecessarily. See https://crbug.com/760720
  const char kUnsupportedKeySystemOrConfigMessage[] =
      "Unsupported keySystem or supportedConfigurations.";

  request.RequestNotSupported(kUnsupportedKeySystemOrConfigMessage);
}

WebEncryptedMediaClientImpl::Reporter* WebEncryptedMediaClientImpl::GetReporter(
    const blink::WebString& key_system) {
  // Assumes that empty will not be found by GetKeySystemNameForUMA().
  // TODO(sandersd): Avoid doing ASCII conversion more than once.
  std::string key_system_ascii;
  if (key_system.ContainsOnlyASCII())
    key_system_ascii = key_system.Ascii();

  // Return a per-frame singleton so that UMA reports will be once-per-frame.
  std::string uma_name = GetKeySystemNameForUMA(key_system_ascii);
  std::unique_ptr<Reporter>& reporter = reporters_[uma_name];
  if (!reporter)
    reporter = std::make_unique<Reporter>(uma_name);
  return reporter.get();
}

}  // namespace media
