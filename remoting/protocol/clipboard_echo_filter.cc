// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_echo_filter.h"

#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

ClipboardEchoFilter::ClipboardEchoFilter()
    : host_stub_(nullptr),
      client_stub_(nullptr),
      client_filter_(this),
      host_filter_(this) {}

ClipboardEchoFilter::~ClipboardEchoFilter() = default;

void ClipboardEchoFilter::set_client_stub(ClipboardStub* client_stub) {
  client_stub_ = client_stub;
}

void ClipboardEchoFilter::set_host_stub(ClipboardStub* host_stub) {
  host_stub_ = host_stub;
}

ClipboardStub* ClipboardEchoFilter::client_filter() {
  return &client_filter_;
}

ClipboardStub* ClipboardEchoFilter::host_filter() {
  return &host_filter_;
}

void ClipboardEchoFilter::InjectClipboardEventToClient(
    const ClipboardEvent& event) {
  if (!client_stub_) {
    return;
  }
  if (event.mime_type() == client_latest_mime_type_ &&
      event.data() == client_latest_data_) {
    return;
  }
  client_stub_->InjectClipboardEvent(event);
}

void ClipboardEchoFilter::InjectClipboardEventToHost(
    const ClipboardEvent& event) {
  client_latest_mime_type_ = event.mime_type();
  client_latest_data_ = event.data();
  if (host_stub_) {
    host_stub_->InjectClipboardEvent(event);
  }
}

ClipboardEchoFilter::ClientFilter::ClientFilter(ClipboardEchoFilter* filter)
    : filter_(filter) {}

void ClipboardEchoFilter::ClientFilter::InjectClipboardEvent(
    const ClipboardEvent& event) {
  filter_->InjectClipboardEventToClient(event);
}

ClipboardEchoFilter::HostFilter::HostFilter(ClipboardEchoFilter* filter)
    : filter_(filter) {}

void ClipboardEchoFilter::HostFilter::InjectClipboardEvent(
    const ClipboardEvent& event) {
  filter_->InjectClipboardEventToHost(event);
}

}  // namespace remoting::protocol
