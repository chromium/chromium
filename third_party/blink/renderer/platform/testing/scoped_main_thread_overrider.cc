// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_main_thread_overrider.h"

namespace blink {

ScopedMainThreadOverrider::ScopedMainThreadOverrider(
    std::unique_ptr<Thread> main_thread)
    : original_main_thread_(Thread::SetMainThread(std::move(main_thread))) {}

ScopedMainThreadOverrider::~ScopedMainThreadOverrider() {
  Thread::SetMainThread(std::move(original_main_thread_));
}

}  // namespace blink
