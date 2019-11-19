// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

namespace gin {

Runner::Runner() {}

Runner::~Runner() = default;

Runner::Scope::Scope(Runner* runner)
    : isolate_scope_(runner->GetContextHolder()->isolate()),
      handle_scope_(runner->GetContextHolder()->isolate()),
      scope_(runner->GetContextHolder()->context()) {
}

Runner::Scope::~Scope() = default;

}  // namespace gin
