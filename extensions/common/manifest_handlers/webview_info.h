// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_

#include <memory>
#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

class PartitionItem;

// A class to hold the <webview> accessible extension resources
// that may be specified in the manifest of an extension using the
// "webview" key.
class WebviewInfo : public Extension::ManifestData {
 public:
  // Returns true if |extension|'s resource at |relative_path| is accessible
  // from the WebView partition with ID |partition_id|.
  static bool IsResourceWebviewAccessible(const Extension* extension,
                                          const std::string& partition_id,
                                          const std::string& relative_path);
  // Returns true if the given |extension| has any webview accessible
  // resources in the given |partition_id|.
  static bool HasWebviewAccessibleResources(const Extension& extension,
                                            const std::string& partition_id);

  // Define out of line constructor/destructor to please Clang.
  explicit WebviewInfo(const ExtensionId& extension_id);

  WebviewInfo(const WebviewInfo&) = delete;
  WebviewInfo& operator=(const WebviewInfo&) = delete;

  ~WebviewInfo() override;

  void AddPartitionItem(std::unique_ptr<PartitionItem> item);

 private:
  ExtensionId extension_id_;
  std::vector<std::unique_ptr<PartitionItem>> partition_items_;
};

// Parses the "webview" manifest key.
class WebviewHandler : public ManifestHandler {
 public:
  WebviewHandler();

  WebviewHandler(const WebviewHandler&) = delete;
  WebviewHandler& operator=(const WebviewHandler&) = delete;

  ~WebviewHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_
