// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PASS_AS_SPAN_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PASS_AS_SPAN_TEST_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class PassAsSpanTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PassAsSpanTest() = default;
  ~PassAsSpanTest() override = default;

  uint64_t testBufferSource(base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testUnlimitedBufferSource(base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testSharedBufferSource(base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testUnlimitedSharedBufferSource(
      base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testTypedArray(base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testUnlimitedTypedArray(base::span<const uint8_t> span) const {
    return span.size();
  }
  uint64_t testTypedArrayOrSequence(base::span<const int32_t> span) const {
    return span.size();
  }
  uint64_t testUnlimitedTypedArrayOrSequence(
      base::span<const int32_t> span) const {
    return span.size();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_PASS_AS_SPAN_TEST_H_
