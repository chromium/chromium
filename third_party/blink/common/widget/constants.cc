// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/constants.h"

namespace blink {

const int kMinimumWindowSize = 100;

// TODO(b/307160156, b/307182741); Investigate where else is the window size
// limited to be able to drop this even more until 9 instead 29.
const int kMinimumBorderlessWindowSize = 29;

const base::TimeDelta kNewContentRenderingDelay = base::Seconds(4);

}  // namespace blink
