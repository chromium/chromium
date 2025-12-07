// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace blink {

int* const CountCopy::kDeletedValue =
    reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

int DummyRefCounted::ref_invokes_count_ = 0;

int* const ValueInstanceCountBase::kDeletedValue =
    reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

HashSet<void*> g_constructed_wrapped_ints;

unsigned LivenessCounter::live_ = 0;

}  // namespace blink
