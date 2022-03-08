// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/error_callback_support.h"

#include "sql/database.h"
#include "sql/statement.h"

namespace sql {

void CaptureErrorCallback(int* error_pointer, int error, Statement* stmt) {
  *error_pointer = error;
}

ScopedErrorCallback::ScopedErrorCallback(Database* db,
                                         Database::ErrorCallback callback)
    : db_(db) {
  DCHECK(!callback.is_null());
  db_->set_error_callback(std::move(callback));
}

ScopedErrorCallback::~ScopedErrorCallback() {
  db_->reset_error_callback();
}

}  // namespace
