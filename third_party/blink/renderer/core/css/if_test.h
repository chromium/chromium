// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_TEST_H_

#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"

namespace blink {

// https://drafts.csswg.org/css-values-5/#typedef-if-test
class IfTest {
  STACK_ALLOCATED();

 public:
  explicit IfTest(const MediaQueryExpNode* style_test)
      : style_test_(style_test), media_test_(nullptr) {}
  explicit IfTest(const MediaQuery* media_test)
      : style_test_(nullptr), media_test_(media_test) {}

  const MediaQueryExpNode* GetStyleTest() { return style_test_; }

  const MediaQuery* GetMediaTest() { return media_test_; }

 private:
  const MediaQueryExpNode* style_test_;
  const MediaQuery* media_test_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_TEST_H_
