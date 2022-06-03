// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
#define EXTENSIONS_BROWSER_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"

namespace content {
class BrowserContext;
}

namespace apps {
struct FileHandler;
struct FileHandlerInfo;
using FileHandlers = std::vector<FileHandler>;
}

namespace extensions {

struct EntryInfo;
struct FileHandlerMatch;
struct GrantedFileEntry;

// TODO(michaelpg,benwells): move this to an app-specific namespace and
// directory.
namespace app_file_handler_util {

extern const char kInvalidParameters[];
extern const char kSecurityError[];

class WebAppFileHandlerMatch {
 public:
  explicit WebAppFileHandlerMatch(const apps::FileHandler* file_handler);
  ~WebAppFileHandlerMatch();

  const apps::FileHandler& file_handler() const;

  // Returns true if |file_handler_| matched an entry on MIME type.
  bool matched_mime_type() const;

  // Returns true if |file_handler_| matched an entry on file extension.
  bool matched_file_extension() const;

  // Returns whether |file_handler_| can handle |entry| on either MIME type or
  // file extension, and sets the values of |matched_mime_type_| and
  // |matched_file_extension_|.
  bool DoMatch(const EntryInfo& entry);

 private:
  const apps::FileHandler* const file_handler_;
  bool matched_mime_type_ = false;
  bool matched_file_extension_ = false;
};

// Returns the file handler with the specified |handler_id|, or NULL if there
// is no such handler.
const apps::FileHandlerInfo* FileHandlerForId(const Extension& app,
                                              const std::string& handler_id);

// Returns the handlers that can handle all files in |entries|
// along with metadata about how the handler matched (MIME or file extension)
std::vector<FileHandlerMatch> FindFileHandlerMatchesForEntries(
    const Extension& extension,
    const std::vector<EntryInfo>& entries);

// Returns the handlers that can handle all files in |entries|
// along with metadata about how the handler matched (MIME or file)
std::vector<FileHandlerMatch> MatchesFromFileHandlersForEntries(
    const FileHandlersInfo& file_handlers,
    const std::vector<EntryInfo>& entries);

// Returns the apps::FileHandlers that can handle all files in |entries", along
// with metadata about how the handler matched (MIME type or file extension).
std::vector<WebAppFileHandlerMatch> MatchesFromWebAppFileHandlersForEntries(
    const apps::FileHandlers& file_handlers,
    const std::vector<EntryInfo>& entries);

bool FileHandlerCanHandleEntry(const apps::FileHandlerInfo& handler,
                               const EntryInfo& entry);

bool WebAppFileHandlerCanHandleEntry(const apps::FileHandler& handler,
                                     const EntryInfo& entry);

// Creates a new file entry and allows |renderer_id| to access |path|. This
// registers a new file system for |path|.
GrantedFileEntry CreateFileEntry(content::BrowserContext* context,
                                 const Extension* extension,
                                 int renderer_id,
                                 const base::FilePath& path,
                                 bool is_directory);

// |directory_paths| contain the set of directories out of |paths|.
// For directories it makes sure they exist at their corresponding |paths|,
// while for regular files it makes sure they exist (i.e. not links) at |paths|,
// creating files if needed. If result is successful it calls |on_success|,
// otherwise calls |on_failure|.
void PrepareFilesForWritableApp(
    const std::vector<base::FilePath>& paths,
    content::BrowserContext* context,
    const std::set<base::FilePath>& directory_paths,
    base::OnceClosure on_success,
    base::OnceCallback<void(const base::FilePath&)> on_failure);

// Returns whether |extension| has the fileSystem.write permission.
bool HasFileSystemWritePermission(const Extension* extension);

// Validates a file entry and populates |file_path| with the absolute path if it
// is valid.
bool ValidateFileEntryAndGetPath(const std::string& filesystem_name,
                                 const std::string& filesystem_path,
                                 int render_process_id,
                                 base::FilePath* file_path,
                                 std::string* error);

}  // namespace app_file_handler_util

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_HANDLERS_APP_FILE_HANDLER_UTIL_H_
