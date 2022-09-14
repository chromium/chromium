// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_SPARSE_CONTROL_H_
#define NET_DISK_CACHE_BLOCKFILE_SPARSE_CONTROL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/blockfile/bitmap.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/disk_cache.h"

namespace net {
class IOBuffer;
class DrainableIOBuffer;
}

namespace disk_cache {

class EntryImpl;

// This class provides support for the sparse capabilities of the disk cache.
// Basically, sparse IO is directed from EntryImpl to this class, and we split
// the operation into multiple small pieces, sending each one to the
// appropriate entry. An instance of this class is associated with each entry
// used directly for sparse operations (the entry passed in to the constructor).
class SparseControl {
 public:
  typedef net::CompletionOnceCallback CompletionOnceCallback;

  // The operation to perform.
  enum SparseOperation {
    kNoOperation,
    kReadOperation,
    kWriteOperation,
    kGetRangeOperation
  };

  explicit SparseControl(EntryImpl* entry);

  SparseControl(const SparseControl&) = delete;
  SparseControl& operator=(const SparseControl&) = delete;

  ~SparseControl();

  // Initializes the object for the current entry. If this entry already stores
  // sparse data, or can be used to do it, it updates the relevant information
  // on disk and returns net::OK. Otherwise it returns a net error code.
  int Init();

  // Performs a quick test to see if the entry is sparse or not, without
  // generating disk IO (so the answer provided is only a best effort).
  bool CouldBeSparse() const;

  // Performs an actual sparse read or write operation for this entry. |op| is
  // the operation to perform, |offset| is the desired sparse offset, |buf| and
  // |buf_len| specify the actual data to use and |callback| is the callback
  // to use for asynchronous operations. See the description of the Read /
  // WriteSparseData for details about the arguments. The return value is the
  // number of bytes read or written, or a net error code.
  int StartIO(SparseOperation op,
              int64_t offset,
              net::IOBuffer* buf,
              int buf_len,
              CompletionOnceCallback callback);

  // Implements Entry::GetAvailableRange().
  RangeResult GetAvailableRange(int64_t offset, int len);

  // Cancels the current sparse operation (if any).
  void CancelIO();

  // Returns OK if the entry can be used for new IO or ERR_IO_PENDING if we are
  // busy. If the entry is busy, we'll invoke the callback when we are ready
  // again. See disk_cache::Entry::ReadyToUse() for more info.
  int ReadyToUse(CompletionOnceCallback completion_callback);

  // Deletes the children entries of |entry|.
  static void DeleteChildren(EntryImpl* entry);

 private:
  // Creates a new sparse entry or opens an aready created entry from disk.
  // These methods just read / write the required info from disk for the current
  // entry, and verify that everything is correct. The return value is a net
  // error code.
  int CreateSparseEntry();
  int OpenSparseEntry(int data_len);

  // Opens and closes a child entry. A child entry is a regular EntryImpl object
  // with a key derived from the key of the resource to store and the range
  // stored by that child.
  bool OpenChild();
  void CloseChild();
  std::string GenerateChildKey();

  // Deletes the current child and continues the current operation (open).
  bool KillChildAndContinue(const std::string& key, bool fatal);

  // Continues the current operation (open) without a current child.
  bool ContinueWithoutChild(const std::string& key);

  // Returns true if the required child is tracked by the parent entry, i.e. it
  // was already created.
  bool ChildPresent();

  // Sets the bit for the current child to the provided |value|. In other words,
  // starts or stops tracking this child.
  void SetChildBit(bool value);

  // Writes to disk the tracking information for this entry.
  void WriteSparseData();

  // Verify that the range to be accessed for the current child is appropriate.
  // Returns false if an error is detected or there is no need to perform the
  // current IO operation (for instance if the required range is not stored by
  // the child).
  bool VerifyRange();

  // Updates the contents bitmap for the current range, based on the result of
  // the current operation.
  void UpdateRange(int result);

  // Returns the number of bytes stored at |block_index|, if its allocation-bit
  // is off (because it is not completely filled).
  int PartialBlockLength(int block_index) const;

  // Initializes the sparse info for the current child.
  void InitChildData();

  // Iterates through all the children needed to complete the current operation.
  void DoChildrenIO();

  // Performs a single operation with the current child. Returns true when we
  // should move on to the next child and false when we should interrupt our
  // work.
  bool DoChildIO();

  // Performs the required work for GetAvailableRange for one child.
  int DoGetAvailableRange();

  // Performs the required work after a single IO operations finishes.
  void DoChildIOCompleted(int result);

  // Invoked by the callback of asynchronous operations.
  void OnChildIOCompleted(int result);

  // Reports to the user that we are done.
  void DoUserCallback();
  void DoAbortCallbacks();

  raw_ptr<EntryImpl> entry_;        // The sparse entry.
  scoped_refptr<EntryImpl> child_;  // The current child entry.
  SparseOperation operation_ = kNoOperation;
  bool pending_ = false;  // True if any child IO operation returned pending.
  bool finished_ = false;
  bool init_ = false;
  bool range_found_ = false;  // True if GetAvailableRange found something.
  bool abort_ = false;  // True if we should abort the current operation ASAP.

  SparseHeader sparse_header_;  // Data about the children of entry_.
  Bitmap children_map_;  // The actual bitmap of children.
  SparseData child_data_;  // Parent and allocation map of child_.
  Bitmap child_map_;  // The allocation map as a bitmap.

  CompletionOnceCallback user_callback_;
  std::vector<CompletionOnceCallback> abort_callbacks_;
  int64_t offset_ = 0;  // Current sparse offset.
  scoped_refptr<net::DrainableIOBuffer> user_buf_;
  int buf_len_ = 0;       // Bytes to read or write.
  int child_offset_ = 0;  // Offset to use for the current child.
  int child_len_ = 0;     // Bytes to read or write for this child.
  int result_ = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_SPARSE_CONTROL_H_
