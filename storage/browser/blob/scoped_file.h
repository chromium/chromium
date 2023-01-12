// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_SCOPED_FILE_H_
#define STORAGE_BROWSER_BLOB_SCOPED_FILE_H_

#include <map>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class TaskRunner;
}

namespace storage {

// A scoped reference for a FilePath that can optionally schedule the file
// to be deleted and/or to notify a consumer when it is going to be scoped out.
// Consumers can pass the ownership of a ScopedFile via std::move().
//
// TODO(kinuko): Probably this can be moved under base or somewhere more
// common place.
class COMPONENT_EXPORT(STORAGE_BROWSER) ScopedFile {
 public:
  using ScopeOutCallback = base::OnceCallback<void(const base::FilePath&)>;
  enum ScopeOutPolicy {
    DELETE_ON_SCOPE_OUT,
    DONT_DELETE_ON_SCOPE_OUT,
  };

  ScopedFile();

  // |file_task_runner| is used to schedule a file deletion if |policy|
  // is DELETE_ON_SCOPE_OUT.
  ScopedFile(const base::FilePath& path,
             ScopeOutPolicy policy,
             scoped_refptr<base::TaskRunner> file_task_runner);

  ScopedFile(ScopedFile&& other);
  ScopedFile& operator=(ScopedFile&& rhs) {
    MoveFrom(rhs);
    return *this;
  }

  ScopedFile(const ScopedFile&) = delete;
  ScopedFile& operator=(const ScopedFile&) = delete;

  ~ScopedFile();

  // The |callback| is fired on |callback_runner| when the final reference
  // of this instance is released.
  // If release policy is DELETE_ON_SCOPE_OUT the
  // callback task(s) is/are posted before the deletion is scheduled.
  // The callbacks are posted in reverse of the order they were added, as LIFO
  // generally makes most sense for cleanup work.
  void AddScopeOutCallback(ScopeOutCallback callback,
                           base::TaskRunner* callback_runner);

  // The full file path.
  const base::FilePath& path() const { return path_; }

  ScopeOutPolicy policy() const { return scope_out_policy_; }

  // Releases the file. After calling this, this instance will hold
  // an empty file path and scoping out won't make any file deletion
  // or callback dispatch. (If an owned pointer is attached to any of
  // callbacks the pointer will be deleted.)
  base::FilePath Release();

  void Reset();

 private:
  // Performs destructive move from |other| to this.
  void MoveFrom(ScopedFile& other);

  base::FilePath path_;
  ScopeOutPolicy scope_out_policy_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  std::vector<std::pair<ScopeOutCallback, scoped_refptr<base::TaskRunner>>>
      scope_out_callbacks_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_SCOPED_FILE_H_
