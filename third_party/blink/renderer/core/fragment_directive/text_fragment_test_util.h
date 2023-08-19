// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_TEST_UTIL_H_

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TextFragmentAnchorTestBase : public SimTest {
 public:
  // SimTest overrides
  void SetUp() override;
  void TearDown() override;

  void RunAsyncMatchingTasks();
  void RunUntilTextFragmentFinalization();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAGMENT_DIRECTIVE_TEXT_FRAGMENT_TEST_UTIL_H_
