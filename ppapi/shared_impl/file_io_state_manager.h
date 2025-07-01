// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_IO_STATE_MANAGER_H_
#define PPAPI_SHARED_IMPL_FILE_IO_STATE_MANAGER_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// FileIOStateManager is a helper class that maintains the state of operations.
// For example, some operations are mutually exclusive, meaning that an
// operation could be rejected because of the current pending operation. Also,
// most of the operations only work when the file has been opened.
class PPAPI_SHARED_EXPORT FileIOStateManager {
 public:
  FileIOStateManager();

  FileIOStateManager(const FileIOStateManager&) = delete;
  FileIOStateManager& operator=(const FileIOStateManager&) = delete;

  ~FileIOStateManager();

  enum OperationType {
    // There is no pending operation right now.
    OPERATION_NONE,

    // If there are pending reads, any other kind of async operation is not
    // allowed.
    OPERATION_READ,

    // If there are pending writes, any other kind of async operation is not
    // allowed.
    OPERATION_WRITE,

    // If there is a pending operation that is neither read nor write, no
    // further async operation is allowed.
    OPERATION_EXCLUSIVE
  };

  OperationType get_pending_operation() const { return pending_op_; }

  void SetOpenSucceed();

  // Called at the beginning of each operation. It is responsible to make sure
  // that state is correct. For example, some operations are only valid after
  // the file is opened, or operations might need to run exclusively.
  //
  // It returns |PP_OK| on success, or |PP_ERROR_...| for various reasons.
  int32_t CheckOperationState(OperationType new_op, bool should_be_open);

  // Marks the state of current operations as started or finished.
  void SetPendingOperation(OperationType op);
  void SetOperationFinished();

 private:
  int num_pending_ops_;
  OperationType pending_op_;

  // Set to true when the file has been successfully opened.
  bool file_open_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_IO_STATE_MANAGER_H_
