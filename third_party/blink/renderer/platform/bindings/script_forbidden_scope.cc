// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace blink {

unsigned ScriptForbiddenScope::g_main_thread_counter_ = 0;
unsigned ScriptForbiddenScope::g_blink_lifecycle_counter_ = 0;

ABSL_CONST_INIT thread_local unsigned script_forbidden_counter = 0;

unsigned& ScriptForbiddenScope::GetMutableCounter() {
  return IsMainThread() ? g_main_thread_counter_ : script_forbidden_counter;
}

}  // namespace blink
