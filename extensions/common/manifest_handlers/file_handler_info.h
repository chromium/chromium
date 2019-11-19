// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

using FileHandlersInfo = std::vector<apps::FileHandlerInfo>;

struct FileHandlerMatch {
  FileHandlerMatch();
  ~FileHandlerMatch();
  const apps::FileHandlerInfo* handler = nullptr;

  // True if the handler matched on MIME type
  bool matched_mime = false;

  // True if the handler matched on file extension
  bool matched_file_extension = false;
};

struct FileHandlers : public Extension::ManifestData {
  FileHandlers();
  ~FileHandlers() override;

  FileHandlersInfo file_handlers;

  static const FileHandlersInfo* GetFileHandlers(const Extension* extension);
};

// Parses the "file_handlers" manifest key.
class FileHandlersParser : public ManifestHandler {
 public:
  FileHandlersParser();
  ~FileHandlersParser() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(FileHandlersParser);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
