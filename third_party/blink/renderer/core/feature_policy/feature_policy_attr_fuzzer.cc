// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  WTF::Vector<WTF::String> messages;
  // TODO(csharrison): Be smarter about parsing these origins for performance.
  scoped_refptr<const blink::SecurityOrigin> parent_origin =
      blink::SecurityOrigin::CreateFromString("https://example.com/");
  scoped_refptr<const blink::SecurityOrigin> child_origin =
      blink::SecurityOrigin::CreateFromString("https://example.net/");
  blink::FeaturePolicyParser::ParseAttribute(WTF::String(data, size),
                                             parent_origin.get(),
                                             child_origin.get(), &messages);
  return 0;
}
