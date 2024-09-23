// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_COMPOSITE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_COMPOSITE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"

namespace storage {

// Manages multiple CopyOrMoveDelegate instances and runs the same method on all
// of them if it is called on this. This class takes the ownership of the other
// instances.
// The saved instances are always called in the same order as they were added.
// For methods with StatusCallback all instances are called even if an error
// occurs. The callback of the composite is called with the first error (order
// of execution) or with File::FILE_OK if no error occurred.

class COMPONENT_EXPORT(STORAGE_BROWSER) CopyOrMoveHookDelegateComposite
    : public CopyOrMoveHookDelegate {
 public:
  // If parent is no CopyOrMoveHookDelegateComposite, a new one is created and
  // parent and child are added. If parent is already a composite, child is
  // added to this. In both cases the composite is returned.
  static std::unique_ptr<CopyOrMoveHookDelegate> CreateOrAdd(
      std::unique_ptr<CopyOrMoveHookDelegate> parent,
      std::unique_ptr<CopyOrMoveHookDelegate> child);

  CopyOrMoveHookDelegateComposite();

  ~CopyOrMoveHookDelegateComposite() override;

  void OnBeginProcessFile(const FileSystemURL& source_url,
                          const FileSystemURL& destination_url,
                          StatusCallback callback) override;

  void OnBeginProcessDirectory(const FileSystemURL& source_url,
                               const FileSystemURL& destination_url,
                               StatusCallback callback) override;

  void OnProgress(const FileSystemURL& source_url,
                  const FileSystemURL& destination_url,
                  int64_t size) override;

  void OnError(const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback) override;

  void OnEndCopy(const FileSystemURL& source_url,
                 const FileSystemURL& destination_url) override;

  void OnEndMove(const FileSystemURL& source_url,
                 const FileSystemURL& destination_url) override;

  void OnEndRemoveSource(const FileSystemURL& source_url) override;

 private:
  friend class CopyOrMoveHookDelegateCompositeTest;

  explicit CopyOrMoveHookDelegateComposite(
      std::unique_ptr<CopyOrMoveHookDelegate> delegate);

  void Add(std::unique_ptr<CopyOrMoveHookDelegate> delegate);

  friend std::unique_ptr<CopyOrMoveHookDelegateComposite>
  std::make_unique<CopyOrMoveHookDelegateComposite>(
      std::unique_ptr<CopyOrMoveHookDelegate>&&);

  std::vector<std::unique_ptr<CopyOrMoveHookDelegate>> delegates_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_COMPOSITE_H_
