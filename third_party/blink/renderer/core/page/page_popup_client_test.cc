// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_popup_client.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(PagePopupClientTest, AddJavaScriptString) {
  test::TaskEnvironment task_environment;
  SegmentedBuffer buffer;
  PagePopupClient::AddJavaScriptString(
      String::FromUTF8("abc\r\n'\"</script>\t\f\v\xE2\x80\xA8\xE2\x80\xA9"),
      buffer);
  const Vector<char> contiguous = std::move(buffer).CopyAs<Vector<char>>();
  EXPECT_EQ(
      "\"abc\\r\\n'\\\"\\x3C/script>\\u0009\\u000C\\u000B\\u2028\\u2029\"",
      std::string(contiguous.data(), contiguous.size()));
}

}  // namespace blink
