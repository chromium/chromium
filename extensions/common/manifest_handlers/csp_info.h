// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
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

  // Content security policy to be used for extension isolated worlds.
  std::string isolated_world_csp;

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

  // Returns the Content Security Policy to be used for extension isolated
  // worlds or null if there is no defined CSP. Note that for extensions,
  // platform apps and legacy packaged apps, a default CSP is used even if the
  // manifest didn't specify one, so null shouldn't be returned for those cases.
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
  ~CSPHandler() override;

  // ManifestHandler override:
  bool Parse(Extension* extension, base::string16* error) override;

 private:
  // Parses the "content_security_policy" dictionary in the manifest.
  bool ParseCSPDictionary(Extension* extension, base::string16* error);

  // Parses the content security policy specified in the manifest for extension
  // pages.
  bool ParseExtensionPagesCSP(Extension* extension,
                              base::string16* error,
                              base::StringPiece manifest_key,
                              bool secure_only,
                              const base::Value* content_security_policy);

  // Parses the content security policy specified in the manifest for isolated
  // worlds.
  bool ParseIsolatedWorldCSP(Extension* extension, base::string16* error);

  // Parses the content security policy specified in the manifest for sandboxed
  // pages. This should be called after ParseExtensionPagesCSP.
  bool ParseSandboxCSP(Extension* extension,
                       base::string16* error,
                       base::StringPiece manifest_key,
                       const base::Value* sandbox_csp);

  // Helper to set the extension pages content security policy manifest data.
  bool SetExtensionPagesCSP(Extension* extension,
                            base::StringPiece manifest_key,
                            bool secure_only,
                            std::string content_security_policy);

  // Helper to set the isolated world content security policy manifest data.
  void SetIsolatedWorldCSP(Extension* extension,
                           std::string isolated_world_csp);

  // Helper to set the sandbox content security policy manifest data.
  void SetSandboxCSP(Extension* extension, std::string sandbox_csp);

  // ManifestHandler overrides:
  bool AlwaysParseForType(Manifest::Type type) const override;
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(CSPHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
