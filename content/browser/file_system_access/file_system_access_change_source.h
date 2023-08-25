// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_

#include <list>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Notifies of changes to the file system within the given `scope`.
// This class must constructed, used, and destroyed on the same sequence.
class CONTENT_EXPORT FileSystemAccessChangeSource {
 public:
  class RawChangeObserver : public base::CheckedObserver {
   public:
    // Naively notifies of all changes from the corresponding change source.
    // These events are _not_ safe to be consumed directly by components that
    // forward events to JavaScript.
    virtual void OnRawChange(FileSystemAccessChangeSource* source,
                             const base::FilePath& relative_path,
                             bool error) = 0;
    virtual void OnSourceBeingDestroyed(
        FileSystemAccessChangeSource* source) = 0;
  };

  // Constructs a change source which notifies of changes within the given
  // `scope`, which must not be null.
  explicit FileSystemAccessChangeSource(FileSystemAccessWatchScope scope);
  virtual ~FileSystemAccessChangeSource();

  void AddObserver(RawChangeObserver* observer);
  void RemoveObserver(RawChangeObserver* observer);

  // Ensures that this change source is ready to watch for changes within its
  // `scope_`. This may fail if the scope cannot be watched.
  // `on_source_initialized` is run with a bool indicating whether setting up
  // this source succeeds.
  // TODO(https://crbug.com/1019297): Assert that this is called before
  // notifying of changes.
  void EnsureInitialized(base::OnceCallback<void(bool)> on_source_initialized);

  base::WeakPtr<FileSystemAccessChangeSource> AsWeakPtr();

  const FileSystemAccessWatchScope& scope() const { return scope_; }

 protected:
  virtual void Initialize(
      base::OnceCallback<void(bool)> on_source_initialized) = 0;

  // Called by subclasses to record changes to watched paths.
  void NotifyOfChange(const base::FilePath& relative_path, bool error);

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void DidInitialize(bool result);

  const FileSystemAccessWatchScope scope_;

  absl::optional<bool> initialization_result_;
  std::list<base::OnceCallback<void(bool)>> initialization_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<RawChangeObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessChangeSource> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_
