// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/mock_font_resource_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

MockFontResourceClient::MockFontResourceClient()
    : font_load_short_limit_exceeded_called_(false),
      font_load_long_limit_exceeded_called_(false) {}

MockFontResourceClient::~MockFontResourceClient() = default;

void MockFontResourceClient::FontLoadShortLimitExceeded(FontResource*) {
  ASSERT_FALSE(font_load_short_limit_exceeded_called_);
  ASSERT_FALSE(font_load_long_limit_exceeded_called_);
  font_load_short_limit_exceeded_called_ = true;
}

void MockFontResourceClient::FontLoadLongLimitExceeded(FontResource*) {
  ASSERT_TRUE(font_load_short_limit_exceeded_called_);
  ASSERT_FALSE(font_load_long_limit_exceeded_called_);
  font_load_long_limit_exceeded_called_ = true;
}

}  // namespace blink
