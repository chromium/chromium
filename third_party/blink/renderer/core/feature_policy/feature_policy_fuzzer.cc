// Copyright 2016 The Chromium Authors. All rights reserved.
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
  // TODO(csharrison): Be smarter about parsing this origin for performance.
  scoped_refptr<const blink::SecurityOrigin> origin =
      blink::SecurityOrigin::CreateFromString("https://example.com/");
  blink::FeaturePolicyParser::ParseHeader(WTF::String(data, size), origin.get(),
                                          &messages);
  return 0;
}
