// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_IO_DATA_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_IO_DATA_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/version.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/common/extension_id.h"

namespace extensions {

using CanonicalRelativePath = content_verifier_utils::CanonicalRelativePath;

// A helper class for keeping track of data for the ContentVerifier that should
// only be accessed on the IO thread.
class ContentVerifierIOData {
 public:
  struct ExtensionData {
    // The following are all canonical paths within an extension to different
    // types of resources.

    // Images used in the browser process (such as icons in the toolbar).
    std::set<CanonicalRelativePath> canonical_browser_image_paths;

    // The extension background page, if any.
    std::optional<CanonicalRelativePath> canonical_background_page_path;

    // The extension background scripts, if any.
    std::set<CanonicalRelativePath> canonical_background_scripts_paths;

    // The extension service worker script, if any.
    std::optional<CanonicalRelativePath> canonical_service_worker_script_path;

    // Content scripts.
    std::set<CanonicalRelativePath> canonical_content_scripts_paths;

    // Set of indexed ruleset paths used by the Declarative Net Request API.
    std::set<CanonicalRelativePath> canonical_indexed_ruleset_paths;

    // The version of the extension.
    base::Version version;

    // The manifest version of the extension.
    int manifest_version = 0;

    ContentVerifierDelegate::VerifierSourceType source_type;

    ExtensionData();

    ExtensionData(ExtensionData&&);
    ExtensionData& operator=(ExtensionData&&);

    ExtensionData(const ExtensionData&) = delete;
    ExtensionData& operator=(const ExtensionData&) = delete;

    ~ExtensionData();
  };

  ContentVerifierIOData();

  ContentVerifierIOData(const ContentVerifierIOData&) = delete;
  ContentVerifierIOData& operator=(const ContentVerifierIOData&) = delete;

  ~ContentVerifierIOData();

  void AddData(const ExtensionId& extension_id, ExtensionData data);
  void RemoveData(const ExtensionId& extension_id);
  void Clear();

  // This should be called on the IO thread, and the return value should not
  // be retained or used on other threads.
  const ExtensionData* GetData(const ExtensionId& extension_id);

 private:
  std::map<ExtensionId, ExtensionData> data_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_IO_DATA_H_
