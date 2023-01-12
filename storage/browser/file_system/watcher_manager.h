// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_WATCHER_MANAGER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_WATCHER_MANAGER_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"

namespace storage {

class FileSystemURL;

// An interface for providing entry observing capability for file system
// backends.
//
// All member functions must be called on the IO thread. Callbacks will be
// called on the IO thread.
//
// It is NOT valid to give null callback to this class, and implementors
// can assume that they don't get any null callbacks.
class WatcherManager {
 public:
  enum ChangeType { CHANGED, DELETED };

  using StatusCallback = base::OnceCallback<void(base::File::Error result)>;
  using NotificationCallback =
      base::RepeatingCallback<void(ChangeType change_type)>;

  WatcherManager(const WatcherManager&) = delete;
  WatcherManager& operator=(const WatcherManager&) = delete;
  virtual ~WatcherManager() = default;

  // Adds an entry watcher. If the |recursive| mode is not supported then
  // FILE_ERROR_INVALID_OPERATION must be returned as an error. If the |url| is
  // already watched with the same |recursive|, or setting up the watcher fails,
  // then |callback| must be called with a specific error code.
  //
  // There may be up to two watchers for the same |url| as well as one of them
  // is recursive, and the other one is not.
  //
  // In case of a success |callback| must be called with the FILE_OK error code.
  // |notification_callback| is called for every change related to the watched
  // directory.
  virtual void AddWatcher(const FileSystemURL& url,
                          bool recursive,
                          StatusCallback callback,
                          NotificationCallback notification_callback) = 0;

  // Removes a watcher represented by |url| in |recursive| mode.
  virtual void RemoveWatcher(const FileSystemURL& url,
                             bool recursive,
                             StatusCallback callback) = 0;

 protected:
  WatcherManager() = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_WATCHER_MANAGER_H_
