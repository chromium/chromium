// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_echo_filter.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;

namespace remoting::protocol {

using test::EqualsClipboardEvent;

static ClipboardEvent MakeClipboardEvent(const std::string& mime_type,
                                         const std::string& data) {
  ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);
  return event;
}

// Check that the filter only filters out events identical to the latest
// clipboard item from the client.
TEST(ClipboardEchoFilterTest, FromClientBlocksIdenticalEventToClient) {
  MockClipboardStub client_stub;
  MockClipboardStub host_stub;

  {
    InSequence s;
    EXPECT_CALL(host_stub,
                InjectClipboardEvent(EqualsClipboardEvent("text", "a")));
    EXPECT_CALL(host_stub,
                InjectClipboardEvent(EqualsClipboardEvent("text", "b")));
    EXPECT_CALL(client_stub,
                InjectClipboardEvent(EqualsClipboardEvent("text", "a")));
    EXPECT_CALL(host_stub,
                InjectClipboardEvent(EqualsClipboardEvent("image", "a")));
    EXPECT_CALL(client_stub,
                InjectClipboardEvent(EqualsClipboardEvent("text", "a")));
  }

  ClipboardEchoFilter filter;
  filter.set_client_stub(&client_stub);
  filter.set_host_stub(&host_stub);

  filter.host_filter()->InjectClipboardEvent(MakeClipboardEvent("text", "a"));
  // The client has sent ("text", "a") to the host, so make sure the filter
  // will stop the host echoing that item back to the client.
  filter.client_filter()->InjectClipboardEvent(MakeClipboardEvent("text", "a"));
  filter.host_filter()->InjectClipboardEvent(MakeClipboardEvent("text", "b"));
  filter.client_filter()->InjectClipboardEvent(MakeClipboardEvent("text", "a"));
  filter.host_filter()->InjectClipboardEvent(MakeClipboardEvent("image", "a"));
  filter.client_filter()->InjectClipboardEvent(MakeClipboardEvent("text", "a"));
}

// Check that the filter will drop events sent to the host, if there is no host
// stub, whether or not there is a client stub.
TEST(ClipboardEchoFilterTest, NoHostStub) {
  MockClipboardStub client_stub;
  MockClipboardStub host_stub;

  EXPECT_CALL(host_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "a")));

  ClipboardEchoFilter filter;
  ClipboardEvent event = MakeClipboardEvent("text", "a");
  filter.host_filter()->InjectClipboardEvent(event);

  filter.set_client_stub(&client_stub);
  filter.host_filter()->InjectClipboardEvent(event);

  filter.set_host_stub(&host_stub);
  filter.host_filter()->InjectClipboardEvent(event);
}

// Check that the filter will drop events sent to the client, if there is no
// client stub, whether or not there is a host stub.
TEST(ClipboardEchoFilter, NoClientStub) {
  MockClipboardStub client_stub;
  MockClipboardStub host_stub;

  EXPECT_CALL(client_stub,
              InjectClipboardEvent(EqualsClipboardEvent("text", "a")));

  ClipboardEchoFilter filter;
  ClipboardEvent event = MakeClipboardEvent("text", "a");
  filter.client_filter()->InjectClipboardEvent(event);

  filter.set_host_stub(&host_stub);
  filter.client_filter()->InjectClipboardEvent(event);

  filter.set_client_stub(&client_stub);
  filter.client_filter()->InjectClipboardEvent(event);
}

}  // namespace remoting::protocol
