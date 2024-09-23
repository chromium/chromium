// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/media/web_content_decryption_module_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/media/cdm_result_promise.h"
#include "third_party/blink/renderer/platform/media/cdm_session_adapter.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_session_impl.h"
#include "url/origin.h"

namespace blink {
namespace {

const char kCreateSessionSessionTypeUMAName[] = "CreateSession.SessionType";
const char kSetServerCertificateUMAName[] = "SetServerCertificate";
const char kGetStatusForPolicyUMAName[] = "GetStatusForPolicy";

}  // namespace

void WebContentDecryptionModuleImpl::Create(
    media::CdmFactory* cdm_factory,
    media::KeySystems* key_systems,
    const WebSecurityOrigin& security_origin,
    const media::CdmConfig& cdm_config,
    WebCdmCreatedCB web_cdm_created_cb) {
  DCHECK(!security_origin.IsNull());

  const auto key_system = cdm_config.key_system;
  DCHECK(!key_system.empty());

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!base::IsStringASCII(key_system)) {
    NOTREACHED_IN_MIGRATION();
    std::move(web_cdm_created_cb)
        .Run(nullptr, media::CreateCdmStatus::kUnsupportedKeySystem);
    return;
  }

  // TODO(ddorwin): This should be a DCHECK.
  if (!key_systems->IsSupportedKeySystem(key_system)) {
    DVLOG(1) << __func__ << "Keysystem '" << key_system
             << "' is not supported.";
    std::move(web_cdm_created_cb)
        .Run(nullptr, media::CreateCdmStatus::kUnsupportedKeySystem);
    return;
  }

  // If opaque security origin, don't try to create the CDM.
  if (security_origin.IsOpaque() || security_origin.ToString() == "null") {
    std::move(web_cdm_created_cb)
        .Run(nullptr, media::CreateCdmStatus::kNotAllowedOnUniqueOrigin);
    return;
  }

  // CdmSessionAdapter::CreateCdm() will keep a reference to |adapter|. Then
  // if WebContentDecryptionModuleImpl is successfully created (returned in
  // |web_cdm_created_cb|), it will keep a reference to |adapter|. Otherwise,
  // |adapter| will be destructed.
  auto adapter = base::MakeRefCounted<CdmSessionAdapter>(key_systems);
  adapter->CreateCdm(cdm_factory, cdm_config, std::move(web_cdm_created_cb));
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    base::PassKey<CdmSessionAdapter>,
    scoped_refptr<CdmSessionAdapter> adapter,
    media::KeySystems* key_systems)
    : adapter_(adapter), key_systems_(key_systems) {}

WebContentDecryptionModuleImpl::~WebContentDecryptionModuleImpl() = default;

std::unique_ptr<WebContentDecryptionModuleSession>
WebContentDecryptionModuleImpl::CreateSession(
    WebEncryptedMediaSessionType session_type) {
  base::UmaHistogramEnumeration(
      adapter_->GetKeySystemUMAPrefix() + kCreateSessionSessionTypeUMAName,
      session_type);
  return adapter_->CreateSession(session_type);
}

void WebContentDecryptionModuleImpl::SetServerCertificate(
    const uint8_t* server_certificate,
    size_t server_certificate_length,
    WebContentDecryptionModuleResult result) {
  DCHECK(server_certificate);
  adapter_->SetServerCertificate(
      std::vector<uint8_t>(server_certificate,
                           server_certificate + server_certificate_length),
      std::make_unique<CdmResultPromise<>>(result,
                                           adapter_->GetKeySystemUMAPrefix(),
                                           kSetServerCertificateUMAName));
}

void WebContentDecryptionModuleImpl::GetStatusForPolicy(
    const WebString& min_hdcp_version_string,
    WebContentDecryptionModuleResult result) {
  std::optional<media::HdcpVersion> min_hdcp_version = std::nullopt;
  if (min_hdcp_version_string.ContainsOnlyASCII()) {
    min_hdcp_version =
        media::MaybeHdcpVersionFromString(min_hdcp_version_string.Ascii());
  }

  if (!min_hdcp_version.has_value()) {
    result.CompleteWithError(kWebContentDecryptionModuleExceptionTypeError, 0,
                             "Invalid HDCP version");
    return;
  }

  adapter_->GetStatusForPolicy(
      min_hdcp_version.value(),
      std::make_unique<CdmResultPromise<media::CdmKeyInformation::KeyStatus>>(
          result, adapter_->GetKeySystemUMAPrefix(),
          kGetStatusForPolicyUMAName));
}

std::unique_ptr<media::CdmContextRef>
WebContentDecryptionModuleImpl::GetCdmContextRef() {
  return adapter_->GetCdmContextRef();
}

media::CdmConfig WebContentDecryptionModuleImpl::GetCdmConfig() const {
  return adapter_->GetCdmConfig();
}

}  // namespace blink
