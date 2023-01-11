// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_filter.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace remoting::protocol {

using test::EqualsClipboardEvent;

static ClipboardEvent MakeClipboardEvent(const std::string& mime_type,
                                         const std::string& data) {
  ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);
  return event;
}

// Verify that the filter passes events on correctly to a configured stub.
TEST(ClipboardFilterTest, EventsPassThroughFilter) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);

  EXPECT_CALL(clipboard_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "foo")));

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter truncates payload if larger than |max_size|.
TEST(ClipboardFilterTest, PayloadLargerThanMaxSize) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);
  clipboard_filter.set_max_size(2);

  EXPECT_CALL(clipboard_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "fo")));

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter does not truncate payload if equal to |max_size|.
TEST(ClipboardFilterTest, PayloadEqualToMaxSize) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);
  clipboard_filter.set_max_size(3);

  EXPECT_CALL(clipboard_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "foo")));

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter does not truncate payload if smaller than |max_size|.
TEST(ClipboardFilterTest, PayloadSmallerThanMaxSize) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);
  clipboard_filter.set_max_size(4);

  EXPECT_CALL(clipboard_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "foo")));

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter ignores events if max_size is 0.
TEST(ClipboardFilterTest, MaxSizeSetToZero) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);
  clipboard_filter.set_max_size(0);

  EXPECT_CALL(clipboard_stub, InjectClipboardEvent(_)).Times(0);

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter ignores events if disabled.
TEST(ClipboardFilterTest, IgnoreEventsIfDisabled) {
  MockClipboardStub clipboard_stub;
  ClipboardFilter clipboard_filter(&clipboard_stub);

  clipboard_filter.set_enabled(false);

  EXPECT_CALL(clipboard_stub, InjectClipboardEvent(_)).Times(0);

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

// Verify that the filter ignores events if not configured.
TEST(ClipboardFilterTest, IgnoreEventsIfNotConfigured) {
  ClipboardFilter clipboard_filter;

  clipboard_filter.InjectClipboardEvent(MakeClipboardEvent("text", "foo"));
}

}  // namespace remoting::protocol
