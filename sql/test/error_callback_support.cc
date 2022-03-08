// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/error_callback_support.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

void CaptureErrorCallback(int* error_pointer, int error, sql::Statement* stmt) {
  *error_pointer = error;
}

ScopedErrorCallback::ScopedErrorCallback(sql::Database* db,
                                         const sql::Database::ErrorCallback& cb)
    : db_(db) {
  // Make sure someone isn't trying to nest things.
  EXPECT_FALSE(db_->has_error_callback());
  db_->set_error_callback(cb);
}

ScopedErrorCallback::~ScopedErrorCallback() {
  db_->reset_error_callback();
}

}  // namespace
