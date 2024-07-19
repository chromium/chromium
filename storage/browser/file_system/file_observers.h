// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_OBSERVERS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_OBSERVERS_H_

#include <stdint.h>

#include "base/component_export.h"

// TODO(kinuko): Split this file into per-observer multiple files.

namespace storage {

class FileSystemURL;

// An abstract interface to observe update operations.
//
// OnStartUpdate and OnEndUpdate are called once for each target url
// before and after following operations regardless of whether the operation
// is made recursively or not (i.e. StartUpdate() will be called only once
// for destination url regardless of whether it is recursive copy or not):
//  CreateFile(), CreateDirectory(),
//  Copy() (destination only),
//  Move() (both for source and destination),
//  Remove(), Write(), Truncate(), TouchFile()
//
// OnUpdate() is called each time the |url| is updated but works only for
// sandboxed files (where usage is tracked).
class COMPONENT_EXPORT(STORAGE_BROWSER) FileUpdateObserver {
 public:
  FileUpdateObserver() = default;
  FileUpdateObserver(const FileUpdateObserver&) = delete;
  FileUpdateObserver& operator=(const FileUpdateObserver&) = delete;
  virtual ~FileUpdateObserver() = default;

  virtual void OnStartUpdate(const FileSystemURL& url) = 0;
  virtual void OnUpdate(const FileSystemURL& url, int64_t delta) = 0;
  virtual void OnEndUpdate(const FileSystemURL& url) = 0;
};

// An abstract interface to observe file access.
// OnAccess is called whenever an operation reads file contents or metadata.
// (It is called only once per operation regardless of whether the operation
// is recursive or not)
class COMPONENT_EXPORT(STORAGE_BROWSER) FileAccessObserver {
 public:
  FileAccessObserver() = default;
  FileAccessObserver(const FileAccessObserver&) = delete;
  FileAccessObserver& operator=(const FileAccessObserver&) = delete;
  virtual ~FileAccessObserver() = default;

  virtual void OnAccess(const FileSystemURL& url) = 0;
};

// An abstract interface to observe file changes.
// Each method of this class is called once per file/directory is created,
// removed or modified.  For recursive operations each method is called for
// each subdirectory/subfile.  Currently ChangeObserver is only supported
// by the local sandbox file system.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileChangeObserver {
 public:
  FileChangeObserver() = default;
  FileChangeObserver(const FileChangeObserver&) = delete;
  FileChangeObserver& operator=(const FileChangeObserver&) = delete;
  virtual ~FileChangeObserver() = default;

  virtual void OnCreateFile(const FileSystemURL& url) = 0;
  // File copy
  virtual void OnCreateFileFrom(const FileSystemURL& url,
                                const FileSystemURL& src) = 0;
  // File move
  virtual void OnMoveFileFrom(const FileSystemURL& url,
                              const FileSystemURL& src) = 0;
  virtual void OnRemoveFile(const FileSystemURL& url) = 0;
  virtual void OnModifyFile(const FileSystemURL& url) = 0;
  virtual void OnCreateDirectory(const FileSystemURL& url) = 0;
  virtual void OnRemoveDirectory(const FileSystemURL& url) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_OBSERVERS_H_
