// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webcontentdecryptionmodule_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/blink/cdm_result_promise.h"
#include "media/blink/cdm_session_adapter.h"
#include "media/blink/webcontentdecryptionmodulesession_impl.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/origin.h"

namespace media {

namespace {

const char kCreateSessionSessionTypeUMAName[] = "CreateSession.SessionType";
const char kSetServerCertificateUMAName[] = "SetServerCertificate";
const char kGetStatusForPolicyUMAName[] = "GetStatusForPolicy";

bool ConvertHdcpVersion(const blink::WebString& hdcp_version_string,
                        HdcpVersion* hdcp_version) {
  if (!hdcp_version_string.ContainsOnlyASCII())
    return false;

  std::string hdcp_version_ascii = hdcp_version_string.Ascii();

  // The strings are specified in the explainer doc:
  // https://github.com/WICG/hdcp-detection/blob/master/explainer.md
  if (hdcp_version_ascii.empty())
    *hdcp_version = HdcpVersion::kHdcpVersionNone;
  else if (hdcp_version_ascii == "1.0")
    *hdcp_version = HdcpVersion::kHdcpVersion1_0;
  else if (hdcp_version_ascii == "1.1")
    *hdcp_version = HdcpVersion::kHdcpVersion1_1;
  else if (hdcp_version_ascii == "1.2")
    *hdcp_version = HdcpVersion::kHdcpVersion1_2;
  else if (hdcp_version_ascii == "1.3")
    *hdcp_version = HdcpVersion::kHdcpVersion1_3;
  else if (hdcp_version_ascii == "1.4")
    *hdcp_version = HdcpVersion::kHdcpVersion1_4;
  else if (hdcp_version_ascii == "2.0")
    *hdcp_version = HdcpVersion::kHdcpVersion2_0;
  else if (hdcp_version_ascii == "2.1")
    *hdcp_version = HdcpVersion::kHdcpVersion2_1;
  else if (hdcp_version_ascii == "2.2")
    *hdcp_version = HdcpVersion::kHdcpVersion2_2;
  else if (hdcp_version_ascii == "2.3")
    *hdcp_version = HdcpVersion::kHdcpVersion2_3;
  else
    return false;

  return true;
}

}  // namespace

void WebContentDecryptionModuleImpl::Create(
    media::CdmFactory* cdm_factory,
    const base::string16& key_system,
    const blink::WebSecurityOrigin& security_origin,
    const CdmConfig& cdm_config,
    WebCdmCreatedCB web_cdm_created_cb) {
  DCHECK(!security_origin.IsNull());
  DCHECK(!key_system.empty());

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!base::IsStringASCII(key_system)) {
    NOTREACHED();
    std::move(web_cdm_created_cb).Run(nullptr, "Invalid keysystem.");
    return;
  }

  // TODO(ddorwin): This should be a DCHECK.
  std::string key_system_ascii = base::UTF16ToASCII(key_system);
  if (!media::KeySystems::GetInstance()->IsSupportedKeySystem(
          key_system_ascii)) {
    std::string message =
        "Keysystem '" + key_system_ascii + "' is not supported.";
    std::move(web_cdm_created_cb).Run(nullptr, message);
    return;
  }

  // If opaque security origin, don't try to create the CDM.
  if (security_origin.IsOpaque() || security_origin.ToString() == "null") {
    std::move(web_cdm_created_cb)
        .Run(nullptr, "EME use is not allowed on unique origins.");
    return;
  }

  // CdmSessionAdapter::CreateCdm() will keep a reference to |adapter|. Then
  // if WebContentDecryptionModuleImpl is successfully created (returned in
  // |web_cdm_created_cb|), it will keep a reference to |adapter|. Otherwise,
  // |adapter| will be destructed.
  scoped_refptr<CdmSessionAdapter> adapter(new CdmSessionAdapter());
  adapter->CreateCdm(cdm_factory, key_system_ascii, cdm_config,
                     std::move(web_cdm_created_cb));
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    scoped_refptr<CdmSessionAdapter> adapter)
    : adapter_(adapter) {
}

WebContentDecryptionModuleImpl::~WebContentDecryptionModuleImpl() = default;

std::unique_ptr<blink::WebContentDecryptionModuleSession>
WebContentDecryptionModuleImpl::CreateSession(
    blink::WebEncryptedMediaSessionType session_type) {
  base::UmaHistogramEnumeration(
      adapter_->GetKeySystemUMAPrefix() + kCreateSessionSessionTypeUMAName,
      session_type);
  return adapter_->CreateSession(session_type);
}

void WebContentDecryptionModuleImpl::SetServerCertificate(
    const uint8_t* server_certificate,
    size_t server_certificate_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(server_certificate);
  adapter_->SetServerCertificate(
      std::vector<uint8_t>(server_certificate,
                           server_certificate + server_certificate_length),
      std::make_unique<CdmResultPromise<>>(result,
                                           adapter_->GetKeySystemUMAPrefix(),
                                           kSetServerCertificateUMAName));
}

void WebContentDecryptionModuleImpl::GetStatusForPolicy(
    const blink::WebString& min_hdcp_version_string,
    blink::WebContentDecryptionModuleResult result) {
  HdcpVersion min_hdcp_version;
  if (!ConvertHdcpVersion(min_hdcp_version_string, &min_hdcp_version)) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionTypeError, 0,
        "Invalid HDCP version");
    return;
  }

  adapter_->GetStatusForPolicy(
      min_hdcp_version,
      std::make_unique<CdmResultPromise<CdmKeyInformation::KeyStatus>>(
          result, adapter_->GetKeySystemUMAPrefix(),
          kGetStatusForPolicyUMAName));
}

std::unique_ptr<CdmContextRef>
WebContentDecryptionModuleImpl::GetCdmContextRef() {
  return adapter_->GetCdmContextRef();
}

std::string WebContentDecryptionModuleImpl::GetKeySystem() const {
  return adapter_->GetKeySystem();
}

CdmConfig WebContentDecryptionModuleImpl::GetCdmConfig() const {
  return adapter_->GetCdmConfig();
}

}  // namespace media
