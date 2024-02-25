// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILES_SERVICE_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILES_SERVICE_INTERFACE_H_

#include <string>

#include "base/files/file_path.h"
#include "extensions/common/extension_id.h"

namespace extensions {

struct SavedFileEntry;

// Provides an LRU of saved file entries that persist across app reloads.
class SavedFilesServiceInterface {
 public:
  virtual ~SavedFilesServiceInterface() {}

  // Registers a file entry with the saved files service, making it eligible to
  // be put into the queue. File entries that are in the retained files queue at
  // object construction are automatically registered.
  virtual void RegisterFileEntry(const ExtensionId& extension_id,
                                 const std::string& id,
                                 const base::FilePath& file_path,
                                 bool is_directory) = 0;

  // If the file with |id| is not in the queue of files to be retained
  // permanently, adds the file to the back of the queue, evicting the least
  // recently used entry at the front of the queue if it is full. If it is
  // already present, moves it to the back of the queue. The |id| must have been
  // registered.
  virtual void EnqueueFileEntry(const ExtensionId& extension_id,
                                const std::string& id) = 0;

  // Returns whether the file entry with the given |id| has been registered.
  virtual bool IsRegistered(const ExtensionId& extension_id,
                            const std::string& id) = 0;

  // Gets a borrowed pointer to the file entry with the specified |id|. Returns
  // nullptr if the file entry has not been registered.
  virtual const SavedFileEntry* GetFileEntry(const ExtensionId& extension_id,
                                             const std::string& id) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILES_SERVICE_INTERFACE_H_
