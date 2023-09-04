// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_FILE_HANDLERS_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_FILE_HANDLERS_INFO_H_

#include <string>
#include <vector>

#include "extensions/common/api/file_handlers.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

using FileHandler = api::file_handlers::FileHandler;

struct WebFileHandler {
  enum class LaunchType {
    kSingleClient,
    kMultipleClients,
  };

  FileHandler file_handler;
  LaunchType launch_type = LaunchType::kSingleClient;

  // Return an enum type instead of the idl string type. This value is currently
  // set to `single-client` if it or nothing is provided, set to
  // `multiple-clients` if that's provided, or errors in any other case.
  LaunchType GetLaunchType() const { return launch_type; }
};

using WebFileHandlersInfo = std::vector<WebFileHandler>;

// Structured contents of the `file_handlers` manifest key.
struct WebFileHandlers : public Extension::ManifestData {
  WebFileHandlers();
  ~WebFileHandlers() override;

  // The list of entries for the web-accessible resources of the extension.
  WebFileHandlersInfo file_handlers;

  static const WebFileHandlersInfo* GetFileHandlers(const Extension& extension);

  static bool HasFileHandlers(const Extension& extension);

  // Support for web file handlers, introduced in MV3 based on the web API named
  // `File Handling Explainer`. True when MV3 and either using
  // the kExtensionWebFileHandlers feature or a QuickOffice extension.
  // TODO(crbug/1179530): Remove after MV2 deprecation.
  static bool SupportsWebFileHandlers(const Extension& extension);

  // Return an enum type instead of the idl string type. This value is currently
  // set to `single-client` if it or nothing is provided, set to
  // `multiple-clients` if that's provided, or errors in any other case.
  // TODO(crbug/1448893): Store enum instead of the string on manifest parse.
  static WebFileHandler::LaunchType GetLaunchType(
      const absl::optional<std::string>& launch_type);
};

// Parses the `file_handlers` manifest key.
class WebFileHandlersParser : public ManifestHandler {
 public:
  WebFileHandlersParser();
  WebFileHandlersParser(const WebFileHandlersParser&) = delete;
  WebFileHandlersParser& operator=(const WebFileHandlersParser&) = delete;
  ~WebFileHandlersParser() override;

  bool Parse(Extension* extension, std::u16string* error) override;

  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_FILE_HANDLERS_INFO_H_
