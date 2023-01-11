// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/scoped_disable_exit_on_dfatal.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"

namespace net::test {

ScopedDisableExitOnDFatal::ScopedDisableExitOnDFatal()
    : assert_handler_(base::BindRepeating(LogAssertHandler)) {}

ScopedDisableExitOnDFatal::~ScopedDisableExitOnDFatal() = default;

// static
void ScopedDisableExitOnDFatal::LogAssertHandler(
    const char* file,
    int line,
    const base::StringPiece message,
    const base::StringPiece stack_trace) {
  // Simply swallow the assert.
}

}  // namespace net::test
