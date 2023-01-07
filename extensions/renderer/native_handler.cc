// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_handler.h"

#include <ostream>

#include "base/check.h"

namespace extensions {

NativeHandler::NativeHandler() : is_valid_(true) {}

NativeHandler::~NativeHandler() {
  CHECK(!is_valid_) << "NativeHandlers must be invalidated before destruction";
}

void NativeHandler::Invalidate() {
  CHECK(is_valid_);
  is_valid_ = false;
}

}  // namespace extensions
