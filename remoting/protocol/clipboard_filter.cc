// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_filter.h"

#include "remoting/proto/internal.pb.h"

namespace remoting::protocol {

ClipboardFilter::ClipboardFilter() = default;

ClipboardFilter::ClipboardFilter(ClipboardStub* clipboard_stub)
    : clipboard_stub_(clipboard_stub) {}

ClipboardFilter::~ClipboardFilter() = default;

void ClipboardFilter::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void ClipboardFilter::InjectClipboardEvent(const ClipboardEvent& event) {
  if (!enabled_ || !clipboard_stub_) {
    return;
  }

  if (max_size_.has_value() && *max_size_ == 0) {
    return;
  }

  if (!max_size_.has_value() || *max_size_ >= event.data().size()) {
    clipboard_stub_->InjectClipboardEvent(event);
  } else {
    ClipboardEvent resized_event(event);
    resized_event.mutable_data()->resize(*max_size_);
    clipboard_stub_->InjectClipboardEvent(resized_event);
  }
}

}  // namespace remoting::protocol
