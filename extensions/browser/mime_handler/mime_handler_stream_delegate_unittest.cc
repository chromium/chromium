// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// A subclass that forgets to override a method must still get a safe
// default, not a permissive one.
TEST(MimeHandlerStreamDelegateTest, DefaultMethods) {
  MimeHandlerStreamDelegate delegate;

  EXPECT_FALSE(delegate.PluginCanSave());
  delegate.SetPluginCanSave(true);
  EXPECT_FALSE(delegate.PluginCanSave());
  EXPECT_TRUE(delegate.ShouldFilterResponseHeadersForHandler());
}

}  // namespace extensions
