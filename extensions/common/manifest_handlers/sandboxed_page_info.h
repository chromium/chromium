// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

struct SandboxedPageInfo : public Extension::ManifestData {
 public:
  SandboxedPageInfo();
  ~SandboxedPageInfo() override;

  // Returns the extension's sandboxed pages.
  static const URLPatternSet& GetPages(const Extension* extension);

  // Returns true if the specified page is sandboxed.
  static bool IsSandboxedPage(const Extension* extension,
                              const std::string& relative_path);

  // Optional list of extension pages that are sandboxed (served from a unique
  // origin with a different Content Security Policy).
  URLPatternSet pages;
};

// Responsible for parsing the "sandbox.pages" manifest key.
// "sandbox.content_security_policy" is parsed by CSPHandler.
class SandboxedPageHandler : public ManifestHandler {
 public:
  SandboxedPageHandler();
  ~SandboxedPageHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_
