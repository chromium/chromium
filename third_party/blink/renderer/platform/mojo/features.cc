// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/features.h"

namespace blink {
// HeapMojo experiments to deprecate kWithoutContextObserver
const base::Feature kHeapMojoUseContextObserver{
    "HeapMojoUseContextObserver", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace blink
