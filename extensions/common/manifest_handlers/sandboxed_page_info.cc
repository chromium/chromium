// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/sandboxed_page_info.h"

#include <stddef.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

namespace keys = extensions::manifest_keys;
namespace errors = manifest_errors;

namespace {

static base::LazyInstance<SandboxedPageInfo>::DestructorAtExit
    g_empty_sandboxed_info = LAZY_INSTANCE_INITIALIZER;

const SandboxedPageInfo& GetSandboxedPageInfo(const Extension* extension) {
  SandboxedPageInfo* info = static_cast<SandboxedPageInfo*>(
      extension->GetManifestData(keys::kSandboxedPages));
  return info ? *info : g_empty_sandboxed_info.Get();
}

}  // namespace

SandboxedPageInfo::SandboxedPageInfo() {
}

SandboxedPageInfo::~SandboxedPageInfo() {
}

const URLPatternSet& SandboxedPageInfo::GetPages(const Extension* extension) {
  return GetSandboxedPageInfo(extension).pages;
}

bool SandboxedPageInfo::IsSandboxedPage(const Extension* extension,
                                    const std::string& relative_path) {
  return extension->ResourceMatches(GetPages(extension), relative_path);
}

SandboxedPageHandler::SandboxedPageHandler() {
}

SandboxedPageHandler::~SandboxedPageHandler() {
}

bool SandboxedPageHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<SandboxedPageInfo> sandboxed_info(new SandboxedPageInfo);

  const base::Value* list_value = nullptr;
  if (!extension->manifest()->GetList(keys::kSandboxedPages, &list_value)) {
    *error = errors::kInvalidSandboxedPagesList;
    return false;
  }

  const base::Value::List& list = list_value->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i].is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidSandboxedPage, base::NumberToString(i));
      return false;
    }
    std::string relative_path = list[i].GetString();
    URLPattern pattern(URLPattern::SCHEME_EXTENSION);
    if (pattern.Parse(extension->url().spec()) !=
        URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidURLPatternError, extension->url().spec());
      return false;
    }
    while (relative_path[0] == '/')
      relative_path = relative_path.substr(1, relative_path.length() - 1);
    pattern.SetPath(pattern.path() + relative_path);
    sandboxed_info->pages.AddPattern(pattern);
  }

  extension->SetManifestData(keys::kSandboxedPages, std::move(sandboxed_info));
  return true;
}

base::span<const char* const> SandboxedPageHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kSandboxedPages};
  return kKeys;
}

}  // namespace extensions
