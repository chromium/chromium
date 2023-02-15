// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_MV3_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_MV3_H_

#include <string>
#include <vector>

#include "extensions/common/api/file_handlers.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

using FileHandler = api::file_handlers::FileHandler;
using FileHandlersInfoMV3 = std::vector<FileHandler>;

// Structured contents of the `file_handlers` manifest key.
struct FileHandlersMV3 : public Extension::ManifestData {
  FileHandlersMV3();
  ~FileHandlersMV3() override;

  // The list of entries for the web-accessible resources of the extension.
  FileHandlersInfoMV3 file_handlers;

  static const FileHandlersInfoMV3* GetFileHandlers(const Extension& extension);

  // Verify that MV3+ file handers are supported.
  // TODO(crbug/1179530): Remove after MV2 deprecation.
  static bool SupportsWebFileHandlers(const int manifest_version);
};

// Parses the `file_handlers` manifest key.
class FileHandlersParserMV3 : public ManifestHandler {
 public:
  FileHandlersParserMV3();
  FileHandlersParserMV3(const FileHandlersParserMV3&) = delete;
  FileHandlersParserMV3& operator=(const FileHandlersParserMV3&) = delete;
  ~FileHandlersParserMV3() override;

  bool Parse(Extension* extension, std::u16string* error) override;

  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_MV3_H_
