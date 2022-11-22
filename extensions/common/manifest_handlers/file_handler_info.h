// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/file_handler_info_mv3.h"

namespace extensions {

using FileHandlersInfo = std::vector<apps::FileHandlerInfo>;

struct FileHandlerMatch {
  FileHandlerMatch();
  ~FileHandlerMatch();
  raw_ptr<const apps::FileHandlerInfo> handler = nullptr;

  // True if the handler matched on MIME type
  bool matched_mime = false;

  // True if the handler matched on file extension
  bool matched_file_extension = false;
};

struct FileHandlers : public Extension::ManifestData {
  FileHandlers();
  ~FileHandlers() override;

  FileHandlersInfo file_handlers;
  FileHandlersInfoMV3 file_handlers_mv3;

  static const FileHandlersInfo* GetFileHandlers(const Extension* extension);
  static const FileHandlersInfoMV3* GetFileHandlersMV3(
      const Extension* extension);
};

// Parses the "file_handlers" manifest key.
class FileHandlersParser : public ManifestHandler {
 public:
  FileHandlersParser();

  FileHandlersParser(const FileHandlersParser&) = delete;
  FileHandlersParser& operator=(const FileHandlersParser&) = delete;

  ~FileHandlersParser() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
