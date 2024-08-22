// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;

  blink::PolicyParserMessageBuffer logger;
  scoped_refptr<const blink::SecurityOrigin> parent_origin =
      blink::SecurityOrigin::CreateFromString("https://example.com/");
  scoped_refptr<const blink::SecurityOrigin> child_origin =
      blink::SecurityOrigin::CreateFromString("https://example.net/");
  blink::PermissionsPolicyParser::ParseAttribute(
      WTF::String(data, static_cast<wtf_size_t>(size)), parent_origin.get(),
      child_origin.get(), logger);
  return 0;
}
