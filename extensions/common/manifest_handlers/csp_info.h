// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_

#include <string>
#include <string_view>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the Content-Security-Policy information.
struct CSPInfo : public Extension::ManifestData {
  explicit CSPInfo(std::string extension_pages_csp);
  ~CSPInfo() override;

  // The Content-Security-Policy for an extension. This is applied to an
  // extension's background contexts i.e. its background page, event page and
  // service worker. Extensions can use Content-Security-Policies to mitigate
  // cross-site scripting and other vulnerabilities.
  std::string extension_pages_csp;

  // Content Security Policy that should be used to enforce the sandbox used
  // by sandboxed pages (guaranteed to have the "sandbox" directive without the
  // "allow-same-origin" token).
  std::string sandbox_csp;

  // Returns the CSP to be used for the extension frames (tabs, popups, iframes)
  // and background contexts, or an empty string if there is no defined CSP.
  // Note that for extensions, platform apps and legacy packaged apps, a default
  // CSP is used even if the manifest didn't specify one, so an empty string
  // shouldn't be returned for those cases.
  static const std::string& GetExtensionPagesCSP(const Extension* extension);

  // Returns the minimum CSP (if any) to append for the `extension`'s resource
  // at the given `relative_path`.
  static const std::string* GetMinimumCSPToAppend(
      const Extension& extension,
      const std::string& relative_path);

  // Returns the Content Security Policy to be used for extension isolated
  // worlds or null if there is no defined CSP.
  static const std::string* GetIsolatedWorldCSP(const Extension& extension);

  // Returns the extension's Content Security Policy for the sandboxed pages.
  static const std::string& GetSandboxContentSecurityPolicy(
      const Extension* extension);

  // Returns the Content Security Policy that the specified resource should be
  // served with.
  static const std::string& GetResourceContentSecurityPolicy(
      const Extension* extension,
      const std::string& relative_path);
};

// Parses "content_security_policy", "app.content_security_policy" and
// "sandbox.content_security_policy" manifest keys.
class CSPHandler : public ManifestHandler {
 public:
  CSPHandler();

  CSPHandler(const CSPHandler&) = delete;
  CSPHandler& operator=(const CSPHandler&) = delete;

  ~CSPHandler() override;

  // ManifestHandler override:
  bool Parse(Extension* extension, std::u16string* error) override;

  // Returns the minimum CSP to use in MV3 extensions. Only exposed for testing.
  static const char* GetMinimumMV3CSPForTesting();
  static const char* GetMinimumUnpackedMV3CSPForTesting();

 private:
  // Parses the "content_security_policy" dictionary in the manifest.
  bool ParseCSPDictionary(Extension* extension, std::u16string* error);

  // Parses the content security policy specified in the manifest for extension
  // pages.
  bool ParseExtensionPagesCSP(Extension* extension,
                              std::u16string* error,
                              std::string_view manifest_key,
                              const base::Value* content_security_policy);

  // Parses the content security policy specified in the manifest for sandboxed
  // pages. This should be called after ParseExtensionPagesCSP. If
  // `allow_remote_sources` is true, this allows the extension to specify remote
  // sources in the sandbox CSP.
  bool ParseSandboxCSP(Extension* extension,
                       std::u16string* error,
                       std::string_view manifest_key,
                       const base::Value* sandbox_csp,
                       bool allow_remote_sources);

  // Helper to set the extension pages content security policy manifest data.
  bool SetExtensionPagesCSP(Extension* extension,
                            std::string_view manifest_key,
                            std::string content_security_policy);

  // Helper to set the sandbox content security policy manifest data.
  void SetSandboxCSP(Extension* extension, std::string sandbox_csp);

  // ManifestHandler overrides:
  bool AlwaysParseForType(Manifest::Type type) const override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
