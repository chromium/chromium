// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include <memory>

namespace blink {

std::atomic_int IntegerObject::destructor_calls{0};

}  // namespace blink
