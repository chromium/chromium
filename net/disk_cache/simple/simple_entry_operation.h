// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

class SimpleEntryImpl;

// SimpleEntryOperation stores the information regarding operations in
// SimpleEntryImpl, between the moment they are issued by users of the backend,
// and the moment when they are executed.
class SimpleEntryOperation {
 public:
  typedef net::CompletionOnceCallback CompletionOnceCallback;

  enum EntryOperationType {
    TYPE_OPEN = 0,
    TYPE_CREATE = 1,
    TYPE_OPEN_OR_CREATE = 2,
    TYPE_CLOSE = 3,
    TYPE_READ = 4,
    TYPE_WRITE = 5,
    TYPE_READ_SPARSE = 6,
    TYPE_WRITE_SPARSE = 7,
    TYPE_GET_AVAILABLE_RANGE = 8,
    TYPE_DOOM = 9,
  };

  // Whether an open/create method has returned an entry (optimistically)
  // already, or if it still needs to be delivered via a callback.
  enum EntryResultState {
    ENTRY_ALREADY_RETURNED = 0,
    ENTRY_NEEDS_CALLBACK = 1,
  };

  SimpleEntryOperation(SimpleEntryOperation&& other);
  ~SimpleEntryOperation();

  static SimpleEntryOperation OpenOperation(SimpleEntryImpl* entry,
                                            EntryResultState result_state,
                                            EntryResultCallback);
  static SimpleEntryOperation CreateOperation(SimpleEntryImpl* entry,
                                              EntryResultState result_state,
                                              EntryResultCallback);
  static SimpleEntryOperation OpenOrCreateOperation(
      SimpleEntryImpl* entry,
      OpenEntryIndexEnum index_state,
      EntryResultState result_state,
      EntryResultCallback);
  static SimpleEntryOperation CloseOperation(SimpleEntryImpl* entry);
  static SimpleEntryOperation ReadOperation(SimpleEntryImpl* entry,
                                            int index,
                                            int offset,
                                            int length,
                                            net::IOBuffer* buf,
                                            CompletionOnceCallback callback);
  static SimpleEntryOperation WriteOperation(SimpleEntryImpl* entry,
                                             int index,
                                             int offset,
                                             int length,
                                             net::IOBuffer* buf,
                                             bool truncate,
                                             bool optimistic,
                                             CompletionOnceCallback callback);
  static SimpleEntryOperation ReadSparseOperation(
      SimpleEntryImpl* entry,
      int64_t sparse_offset,
      int length,
      net::IOBuffer* buf,
      CompletionOnceCallback callback);
  static SimpleEntryOperation WriteSparseOperation(
      SimpleEntryImpl* entry,
      int64_t sparse_offset,
      int length,
      net::IOBuffer* buf,
      CompletionOnceCallback callback);
  static SimpleEntryOperation GetAvailableRangeOperation(
      SimpleEntryImpl* entry,
      int64_t sparse_offset,
      int length,
      RangeResultCallback callback);
  static SimpleEntryOperation DoomOperation(SimpleEntryImpl* entry,
                                            CompletionOnceCallback callback);

  EntryOperationType type() const {
    return static_cast<EntryOperationType>(type_);
  }
  CompletionOnceCallback ReleaseCallback() { return std::move(callback_); }
  EntryResultCallback ReleaseEntryResultCallback() {
    return std::move(entry_callback_);
  }
  RangeResultCallback ReleaseRangeResultCalback() {
    return std::move(range_callback_);
  }

  EntryResultState entry_result_state() { return entry_result_state_; }

  OpenEntryIndexEnum index_state() const { return index_state_; }
  int index() const { return index_; }
  int offset() const { return offset_; }
  int64_t sparse_offset() const { return sparse_offset_; }
  int length() const { return length_; }
  net::IOBuffer* buf() { return buf_.get(); }
  bool truncate() const { return truncate_; }
  bool optimistic() const { return optimistic_; }

 private:
  SimpleEntryOperation(SimpleEntryImpl* entry,
                       net::IOBuffer* buf,
                       CompletionOnceCallback callback,
                       int offset,
                       int64_t sparse_offset,
                       int length,
                       EntryOperationType type,
                       OpenEntryIndexEnum index_state,
                       int index,
                       bool truncate,
                       bool optimistic);

  // This ensures entry will not be deleted until the operation has ran.
  scoped_refptr<SimpleEntryImpl> entry_;
  scoped_refptr<net::IOBuffer> buf_;
  CompletionOnceCallback callback_;

  // Used in open and create operations.
  EntryResultCallback entry_callback_;
  EntryResultState entry_result_state_;

  // Used in write and read operations.
  const int offset_;
  const int64_t sparse_offset_;
  const int length_;

  // Used in get available range operations.
  RangeResultCallback range_callback_;

  const EntryOperationType type_;
  // Used in the "open or create" operation.
  const OpenEntryIndexEnum index_state_;
  // Used in write and read operations.
  const unsigned int index_;
  // Used only in write operations.
  const bool truncate_;
  const bool optimistic_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_
