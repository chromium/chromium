// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_io_state_manager.h"

#include "base/check_op.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

FileIOStateManager::FileIOStateManager()
    : num_pending_ops_(0), pending_op_(OPERATION_NONE), file_open_(false) {}

FileIOStateManager::~FileIOStateManager() {}

void FileIOStateManager::SetOpenSucceed() { file_open_ = true; }

int32_t FileIOStateManager::CheckOperationState(OperationType new_op,
                                                bool should_be_open) {
  if (should_be_open) {
    if (!file_open_)
      return PP_ERROR_FAILED;
  } else {
    if (file_open_)
      return PP_ERROR_FAILED;
  }

  if (pending_op_ != OPERATION_NONE &&
      (pending_op_ != new_op || pending_op_ == OPERATION_EXCLUSIVE))
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void FileIOStateManager::SetPendingOperation(OperationType new_op) {
  DCHECK(pending_op_ == OPERATION_NONE ||
         (pending_op_ != OPERATION_EXCLUSIVE && pending_op_ == new_op));
  pending_op_ = new_op;
  num_pending_ops_++;
}

void FileIOStateManager::SetOperationFinished() {
  DCHECK_GT(num_pending_ops_, 0);
  if (--num_pending_ops_ == 0)
    pending_op_ = OPERATION_NONE;
}

}  // namespace ppapi
