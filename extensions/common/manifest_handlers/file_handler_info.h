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

namespace extensions {

using FileHandlersInfo = std::vector<apps::FileHandlerInfo>;

// When setting up the menus for file open, if a file type has default Chrome
// extension set as the default we used to try to choose a default handler by
// matching against any sniffed MIME type or its file name extension.
//
// If there was no clear 'winner' for being set as the default handler for the
// file type, we'd prefer one of our allowlisted handlers over a handler that
// explicitly matches the file name extension. e.g. an '.ica' file contains
// plain text, but if there is a Chrome extension registered that lists '.ica'
// in its 'file_handlers' in the manifest, it fails to be chosen as default if
// there is a text editor installed that can process MIME types of text/plain.
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

  static const FileHandlersInfo* GetFileHandlers(const Extension* extension);
};

// Parses the "file_handlers" manifest key.
class FileHandlersParser : public ManifestHandler {
 public:
  FileHandlersParser();

  FileHandlersParser(const FileHandlersParser&) = delete;
  FileHandlersParser& operator=(const FileHandlersParser&) = delete;

  ~FileHandlersParser() override;

  bool Parse(Extension* extension, std::u16string* error) override;

  // Validation for Web File Handlers. This method was added for MV3 to enable
  // successful loading with warnings, instead of failing to load with errors.
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
