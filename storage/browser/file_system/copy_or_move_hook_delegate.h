// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"

namespace storage {
class FileSystemURL;

// A delegate to handle different hooks of the CopyOrMove operation.
// Its OnBegin* functions take a callback that should be called when the
// function finishes, potentially notifying of any errors during the execution
// of the function. Similarly, OnError takes a callback that is called when a
// error happens and might be used to skip the error and continue the operation.
//
// The other functions do not take a callback and can asynchronously notify of
// any progress. They are used for progress updates, etc. in
// FileSystemOperation::Copy() and Move().
//
// Note that Move() has both a same-filesystem (1) and a cross-filesystem (2)
// implementation.
// 1) Requires metadata updates. Depending on the underlying implementation:
// - we either only update the metadata of (or in other words, rename) the
// moving directory
// - or the directories are recursively copied + deleted, while the files are
// moved by having their metadata updated.
// 2) Degrades into copy + delete: each entry is copied and deleted
// recursively.
//
// When transferring files, the expected events are as follows:
// Copy: OnBeginProcessFile -> OnProgress -> ... -> OnProgress ->
//   OnEndCopy.
// Move (same-filesystem): OnBeginProcessFile -> OnEndMove.
// Move (cross-filesystem): OnBeginProcessFile -> OnProgress -> ... ->
//   OnProgress -> OnEndCopy -> OnEndRemoveSource.
//
// Here is an example callback sequence of for a copy or a cross-filesystem
// move. Suppose there are a/b/c.txt (100 bytes) and a/b/d.txt (200 bytes),
// and trying to transfer a/ to x/ recursively, then the progress update
// sequence will be (Note that for the root directory, OnBeginProcessFile is
// called instead of OnBeginProcessDirectory):
//
// OnBeginProcessFile a x/a (starting create "a" directory in x/).
// OnEndCopy a x/a (creating "a" directory in x/ is finished).
//
// OnBeginProcessDirectory a/b x/a/b (starting create "b" directory in x/a).
// OnEndCopy a/b x/a/b (creating "b" directory in x/a/ is finished).
//
// OnBeginProcessFile a/b/c.txt x/a/b/c.txt (starting to transfer "c.txt" in
//                                           x/a/b/).
// OnProgress a/b/c.txt x/a/b/c.txt 0 (The first OnProgress's `size`
//                                         should be 0).
// OnProgress a/b/c.txt x/a/b/c.txt 10
//    :
// OnProgress a/b/c.txt x/a/b/c.txt 90
// OnProgress a/b/c.txt x/a/b/c.txt 100 (The last OnProgress's `size`
//                                           should be the size of the file).
// OnEndCopy a/b/c.txt x/a/b/c.txt (transferring "c.txt" is finished).
// OnEndRemoveSource a/b/c.txt ("copy + delete" move case).
//
// OnBeginProcessFile a/b/d.txt x/a/b/d.txt (starting to transfer "d.txt" in
//                                           x/a/b).
// OnProgress a/b/d.txt x/a/b/d.txt 0 (The first OnProgress's
//                                         `size` should be 0).
// OnProgress a/b/d.txt x/a/b/d.txt 10
//    :
// OnProgress a/b/d.txt x/a/b/d.txt 190
// OnProgress a/b/d.txt x/a/b/d.txt 200 (The last OnProgress's `size`
//                                           should be the size of the file).
// OnEndCopy a/b/d.txt x/a/b/d.txt (transferring "d.txt" is finished).
// OnEndRemoveSource a/b/d.txt ("copy + delete" move case).
//
// OnEndRemoveSource a/b ("copy + delete" move case).
//
// OnEndRemoveSource a ("copy + delete" move case).
//
// Note that event sequence of a/b/c.txt and a/b/d.txt can be interlaced,
// because they can be done in parallel. Also OnProgress events are
// optional, so they may not appear. All the progress callback invocations
// should be done before StatusCallback given to the Copy is called. Especially
// if an error is found before the first progress callback invocation, the
// progress callback may NOT be invoked for the copy.
//
class COMPONENT_EXPORT(STORAGE_BROWSER) CopyOrMoveHookDelegate {
 public:
  enum class ErrorAction { kDefault, kSkip };
  using StatusCallback = base::OnceCallback<void(base::File::Error result)>;
  using ErrorCallback = base::OnceCallback<void(ErrorAction)>;

  explicit CopyOrMoveHookDelegate(bool is_composite = false);

  virtual ~CopyOrMoveHookDelegate();

  // OnBeginProcessFile, resp. OnBeginProcessDirectory is called at the start of
  // each copy or move operation for a file, resp. a directory. Note that for
  // the root directory, OnBeginProcessFile is called instead of
  // OnBeginProcessDirectory. This resembles the call order of the
  // RecursiveOperationDelegate.
  virtual void OnBeginProcessFile(const FileSystemURL& source_url,
                                  const FileSystemURL& destination_url,
                                  StatusCallback callback);
  virtual void OnBeginProcessDirectory(const FileSystemURL& source_url,
                                       const FileSystemURL& destination_url,
                                       StatusCallback callback);

  // OnProgress is called periodically during file transfer (not called for
  // same-filesystem move and directory copy/move).
  // `size` is the number of cumulative copied bytes for the currently copied
  // file. Both at beginning and ending of file transfer, PROGRESS event should
  // be called. At beginning, `size` should be 0. At ending, `size` should be
  // the size of the file.
  virtual void OnProgress(const FileSystemURL& source_url,
                          const FileSystemURL& destination_url,
                          int64_t size);

  // OnError is called for any occurring error.
  // `error` is the base::File::Error that was noticed.
  // `callback` might be used to skip/suppress the error (when invoked with
  // kSkip), otherwise should be just called with kDefault.
  // Note that:
  //  * OnError is also called if an OnBeginProcessFile or
  //  OnBeginProcessDirectory function passes an error to their respective
  //  callbacks.
  //  * The operation is paused until the `callback` gets called.
  //  * Using kSkip will continue the operation even if it was run with
  //  ERROR_BEHAVIOR_ABORT, which makes possible to make abort/skip
  //  decision per every error encountered.
  virtual void OnError(const FileSystemURL& source_url,
                       const FileSystemURL& destination_url,
                       base::File::Error error,
                       ErrorCallback callback);

  // OnEndCopy is called for each destination entry that has been successfully
  // copied (for both file and directory). As mentioned before, it is also
  // called for a cross-filesystem move.
  virtual void OnEndCopy(const FileSystemURL& source_url,
                         const FileSystemURL& destination_url);

  // OnEndMove is called for each entry that has been successfully moved (for
  // both file and directory), in the case of a same-filesystem move.
  virtual void OnEndMove(const FileSystemURL& source_url,
                         const FileSystemURL& destination_url);

  // OnEndRemoveSource, applies in the Move() case only, and is called for
  // each source entry that has been successfully removed from its source
  // location (for both file and directory).
  virtual void OnEndRemoveSource(const FileSystemURL& source_url);

 protected:
  friend class CopyOrMoveHookDelegateCompositeTest;
  friend class CopyOrMoveHookDelegateComposite;

  SEQUENCE_CHECKER(sequence_checker_);

  // Indicates if this is an instance of CopyOrMoveHookDelegateComposite.
  const bool is_composite_ = false;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_HOOK_DELEGATE_H_
