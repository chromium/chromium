// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_TEST_H_

#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

// The NGLayoutTest is intended to let all NGFooTest classes easily inherit
// ScopedLayoutNGForTest as well as RenderingTest. The ScopedLayoutNGForTest
// ensures original settings are restored for other tests.
// See http://crbug.com/769541 for more details.
class NGLayoutTest : public RenderingTest, private ScopedLayoutNGForTest {
 public:
  NGLayoutTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client), ScopedLayoutNGForTest(true) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_TEST_H_
