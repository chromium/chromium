// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/csp_info.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/csp_validator.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

using csp_validator::ContentSecurityPolicyIsLegal;
using csp_validator::SanitizeContentSecurityPolicy;

namespace {

const char kDefaultContentSecurityPolicy[] =
    "script-src 'self' blob: filesystem: chrome-extension-resource:; "
    "object-src 'self' blob: filesystem:;";

#define PLATFORM_APP_LOCAL_CSP_SOURCES \
    "'self' blob: filesystem: data: chrome-extension-resource:"

// clang-format off
const char kDefaultPlatformAppContentSecurityPolicy[] =
    // Platform apps can only use local resources by default.
    "default-src 'self' blob: filesystem: chrome-extension-resource:;"
    // For remote resources, they can fetch them via XMLHttpRequest.
    " connect-src * data: blob: filesystem:;"
    // And serve them via data: or same-origin (blob:, filesystem:) URLs
    " style-src " PLATFORM_APP_LOCAL_CSP_SOURCES " 'unsafe-inline';"
    " img-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    " frame-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    " font-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    // Media can be loaded from remote resources since:
    // 1. <video> and <audio> have good fallback behavior when offline or under
    //    spotty connectivity.
    // 2. Fetching via XHR and serving via blob: URLs currently does not allow
    //    streaming or partial buffering.
    " media-src * data: blob: filesystem:;"
    // Scripts are allowed to use WebAssembly
    " script-src 'self' blob: filesystem: chrome-extension-resource:"
    " 'wasm-eval';";
// clang-format on

int GetValidatorOptions(Extension* extension) {
  int options = csp_validator::OPTIONS_NONE;

  // crbug.com/146487
  if (extension->GetType() == Manifest::TYPE_EXTENSION ||
      extension->GetType() == Manifest::TYPE_LEGACY_PACKAGED_APP) {
    options |= csp_validator::OPTIONS_ALLOW_UNSAFE_EVAL;
  }

  // Component extensions can specify an insecure object-src directive. This
  // should be safe because non-NPAPI plugins should load in a sandboxed process
  // and only allow communication via postMessage. Flash is an exception since
  // it allows scripting into the embedder page, but even then it should
  // disallow cross-origin scripting. At some point we may want to consider
  // allowing this publicly.
  if (extensions::Manifest::IsComponentLocation(extension->location()))
    options |= csp_validator::OPTIONS_ALLOW_INSECURE_OBJECT_SRC;

  return options;
}

}  // namespace

CSPInfo::CSPInfo(const std::string& security_policy)
    : content_security_policy(security_policy) {
}

CSPInfo::~CSPInfo() {
}

// static
const std::string& CSPInfo::GetContentSecurityPolicy(
    const Extension* extension) {
  CSPInfo* csp_info = static_cast<CSPInfo*>(
          extension->GetManifestData(keys::kContentSecurityPolicy));
  return csp_info ? csp_info->content_security_policy : base::EmptyString();
}

// static
const std::string& CSPInfo::GetResourceContentSecurityPolicy(
    const Extension* extension,
    const std::string& relative_path) {
  return SandboxedPageInfo::IsSandboxedPage(extension, relative_path) ?
      SandboxedPageInfo::GetContentSecurityPolicy(extension) :
      GetContentSecurityPolicy(extension);
}

CSPHandler::CSPHandler(bool is_platform_app)
    : is_platform_app_(is_platform_app) {
}

CSPHandler::~CSPHandler() {
}

bool CSPHandler::Parse(Extension* extension, base::string16* error) {
  const std::string key = Keys()[0];
  if (!extension->manifest()->HasPath(key)) {
    // TODO(abarth): Should we continue to let extensions override the
    //               default Content-Security-Policy?
    std::string content_security_policy =
        is_platform_app_ ? kDefaultPlatformAppContentSecurityPolicy
                         : kDefaultContentSecurityPolicy;

    CHECK_EQ(
        content_security_policy,
        SanitizeContentSecurityPolicy(content_security_policy,
                                      GetValidatorOptions(extension), NULL));
    extension->SetManifestData(
        keys::kContentSecurityPolicy,
        std::make_unique<CSPInfo>(content_security_policy));
    return true;
  }

  std::string content_security_policy;
  if (!extension->manifest()->GetString(key, &content_security_policy)) {
    *error = base::ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
    return false;
  }
  if (!ContentSecurityPolicyIsLegal(content_security_policy)) {
    *error = base::ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
    return false;
  }
  std::vector<InstallWarning> warnings;
  content_security_policy = SanitizeContentSecurityPolicy(
      content_security_policy, GetValidatorOptions(extension), &warnings);
  extension->AddInstallWarnings(std::move(warnings));

  extension->SetManifestData(
      keys::kContentSecurityPolicy,
      std::make_unique<CSPInfo>(content_security_policy));
  return true;
}

bool CSPHandler::AlwaysParseForType(Manifest::Type type) const {
  if (is_platform_app_)
    return type == Manifest::TYPE_PLATFORM_APP;
  else
    return type == Manifest::TYPE_EXTENSION ||
        type == Manifest::TYPE_LEGACY_PACKAGED_APP;
}

base::span<const char* const> CSPHandler::Keys() const {
  if (is_platform_app_) {
    static constexpr const char* kKeys[] = {
        keys::kPlatformAppContentSecurityPolicy};
    return kKeys;
  }
  static constexpr const char* kKeys[] = {keys::kContentSecurityPolicy};
  return kKeys;
}

}  // namespace extensions
