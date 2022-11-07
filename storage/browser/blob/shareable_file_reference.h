// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_SHAREABLE_FILE_REFERENCE_H_
#define STORAGE_BROWSER_BLOB_SHAREABLE_FILE_REFERENCE_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/scoped_file.h"

namespace storage {

// ShareableFileReference allows consumers to share FileReference for the
// same path if it already exists in its internal map.
// This class is non-thread-safe and all methods must be called on a single
// thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) ShareableFileReference
    : public base::RefCounted<ShareableFileReference> {
 public:
  using FinalReleaseCallback = ScopedFile::ScopeOutCallback;

  enum FinalReleasePolicy {
    DELETE_ON_FINAL_RELEASE = ScopedFile::DELETE_ON_SCOPE_OUT,
    DONT_DELETE_ON_FINAL_RELEASE = ScopedFile::DONT_DELETE_ON_SCOPE_OUT,
  };

  // Returns a ShareableFileReference for the given path, if no reference
  // for this path exists returns nullptr.
  static scoped_refptr<ShareableFileReference> Get(const base::FilePath& path);

  // Returns a ShareableFileReference for the given path, creating a new
  // reference if none yet exists. If there's a pre-existing reference for
  // the path, the policy parameter of this method is ignored.
  static scoped_refptr<ShareableFileReference> GetOrCreate(
      const base::FilePath& path,
      FinalReleasePolicy policy,
      base::TaskRunner* file_task_runner);

  // Returns a ShareableFileReference for the given path of the |scoped_file|,
  // creating a new reference if none yet exists. The ownership of |scoped_file|
  // is passed to this reference.
  // If there's a pre-existing reference for the path, the scope out policy
  // and scope-out-callbacks of the given |scoped_file| is ignored.
  // If the given scoped_file has an empty path (e.g. maybe already
  // released) this returns nullptr reference.
  //
  // TODO(kinuko): Make sure if this behavior is ok, we could alternatively
  // merge callbacks to the existing one.
  static scoped_refptr<ShareableFileReference> GetOrCreate(
      ScopedFile scoped_file);

  ShareableFileReference(const ShareableFileReference&) = delete;
  ShareableFileReference& operator=(const ShareableFileReference&) = delete;

  // The full file path.
  const base::FilePath& path() const { return scoped_file_.path(); }

  // The |callback| is fired when the final reference of this instance
  // is released. If release policy is DELETE_ON_FINAL_RELEASE the
  // callback task(s) is/are posted before the deletion is scheduled.
  // The callbacks are posted in reverse of the order they were added, because
  // LIFO order makes it possible for later cleanup callbacks to restore state
  // earlier added callbacks might rely on (this is relied on by for example
  // code in BlobMemoryController, when shrinking file allocations).
  void AddFinalReleaseCallback(FinalReleaseCallback callback);

 private:
  friend class base::RefCounted<ShareableFileReference>;

  ShareableFileReference(ScopedFile scoped_file);
  ~ShareableFileReference();

  ScopedFile scoped_file_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_SHAREABLE_FILE_REFERENCE_H_
