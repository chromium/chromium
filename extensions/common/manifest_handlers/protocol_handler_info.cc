// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/protocol_handler_info.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/protocol_handlers.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

using ProtocolHandlersManifestKeys = api::protocol_handlers::ManifestKeys;

namespace {

bool IsValidProtocolHandler(const std::string& protocol,
                            const std::string& name,
                            const GURL& url,
                            blink::ProtocolHandlerSecurityLevel security_level,
                            std::vector<InstallWarning>& warnings) {
  // Implementation of the protocol handler arguments normalization steps
  // defined in the spec.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  bool is_valid = true;

  if (name.empty()) {
    warnings.emplace_back(errors::kProtocolHandlerEmptyName);
    is_valid = false;
  }

  // Verify custom handler schemes for errors as described in steps 1 and 2
  if (!blink::IsValidCustomHandlerScheme(protocol, security_level)) {
    warnings.emplace_back(errors::kProtocolHandlerSchemeNotInSafeList);
    is_valid = false;
  }

  switch (blink::IsValidCustomHandlerURLSyntax(url, security_level)) {
    case blink::URLSyntaxErrorCode::kNoError:
      break;
    case blink::URLSyntaxErrorCode::kMissingToken:
      warnings.emplace_back(errors::kProtocolHandlerUrlTokenMissing);
      is_valid = false;
      break;
    case blink::URLSyntaxErrorCode::kInvalidUrl:
      warnings.emplace_back(errors::kProtocolHandlerUrlInvalidSyntax);
      is_valid = false;
      break;
  }

  // Verify custom handler URL security as described in steps 6 and 7
  if (!blink::IsAllowedCustomHandlerURL(url, security_level)) {
    warnings.emplace_back(errors::kProtocolHandlerUntrustworthyScheme);
    is_valid = false;
  }
  url::Origin url_origin = url::Origin::Create(url);
  if (url.is_valid() && url_origin.opaque()) {
    warnings.emplace_back(errors::kProtocolHandlerOpaqueOrigin);
    is_valid = false;
  }

  // TODO(crbug.com/40482153): We need to do a better analysis of the
  // check defined here, based on the security_level and the SameOrigin policy.
  url::Origin origin;
  if (security_level < blink::ProtocolHandlerSecurityLevel::kUntrustedOrigins &&
      !origin.IsSameOriginWith(url)) {
    warnings.emplace_back(errors::kProtocolHandlerIncompabibleOrigins);
    is_valid = false;
  }

  return is_valid;
}

bool SupportsProtocolHandlers(const Extension& extension) {
  return base::FeatureList::IsEnabled(
      extensions_features::kExtensionProtocolHandlers);
}

}  // namespace

ProtocolHandlers::ProtocolHandlers() = default;
ProtocolHandlers::~ProtocolHandlers() = default;

// static
const ProtocolHandlersInfo* ProtocolHandlers::GetProtocolHandlers(
    const Extension& extension) {
  ProtocolHandlers* info = static_cast<ProtocolHandlers*>(
      extension.GetManifestData(keys::kProtocolHandlers));
  DCHECK(!info || SupportsProtocolHandlers(extension));
  return info ? &info->protocol_handlers : nullptr;
}

ProtocolHandlersParser::ProtocolHandlersParser() = default;
ProtocolHandlersParser::~ProtocolHandlersParser() = default;

std::unique_ptr<ProtocolHandlers> ParseEntryList(
    const Extension& extension,
    std::vector<InstallWarning>& install_warnings) {
  std::u16string warning;
  ProtocolHandlersManifestKeys manifest_keys;
  if (!ProtocolHandlersManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, warning)) {
    install_warnings.emplace_back(base::UTF16ToUTF8(warning));
    return nullptr;
  }

  if (manifest_keys.protocol_handlers.empty()) {
    install_warnings.emplace_back(errors::kInvalidProtocolHandlersEmpty);
    return nullptr;
  }

  blink::ProtocolHandlerSecurityLevel security_level =
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures;

  std::unique_ptr<ProtocolHandlers> info = std::make_unique<ProtocolHandlers>();
  for (const auto& protocol_handler : manifest_keys.protocol_handlers) {
    apps::ProtocolHandlerInfo handler;
    DCHECK(!protocol_handler.protocol.empty());
    DCHECK(!protocol_handler.uri_template.empty());
    handler.protocol = protocol_handler.protocol;
    handler.name = protocol_handler.name;
    handler.url = GURL(protocol_handler.uri_template);

    // Validation of Protocol Handlers according to the Custom Handlers section
    // of the HTML spec.
    // https://html.spec.whatwg.org/#normalize-protocol-handler-parameters
    if (IsValidProtocolHandler(handler.protocol, handler.name, handler.url,
                               security_level, install_warnings)) {
      info->protocol_handlers.push_back(handler);
    }
  }
  return info;
}

bool ProtocolHandlersParser::Parse(Extension* extension,
                                   std::u16string* error) {
  CHECK(extension);
  CHECK_GE(extension->manifest_version(), 3);

  if (!SupportsProtocolHandlers(*extension)) {
    return true;
  }

  std::vector<InstallWarning> install_warnings;
  auto info = ParseEntryList(*extension, install_warnings);
  if (info) {
    extension->SetManifestData(keys::kProtocolHandlers, std::move(info));
  }

  extension->AddInstallWarnings(std::move(install_warnings));

  // Allow the extension to be installed, but handlers with warnings will be
  // ignored and not registered as custom handlers.
  return true;
}

base::span<const char* const> ProtocolHandlersParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kProtocolHandlers};
  return kKeys;
}

}  // namespace extensions
