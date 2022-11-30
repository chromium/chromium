// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that receives clipboard events.
// This interface handles some event messages defined in event.proto.

#ifndef REMOTING_PROTOCOL_CLIPBOARD_STUB_H_
#define REMOTING_PROTOCOL_CLIPBOARD_STUB_H_

namespace remoting::protocol {

class ClipboardEvent;

class ClipboardStub {
 public:
  ClipboardStub() = default;

  ClipboardStub(const ClipboardStub&) = delete;
  ClipboardStub& operator=(const ClipboardStub&) = delete;

  virtual ~ClipboardStub() = default;

  // Implementations must not assume the presence of |event|'s fields, nor that
  // |event.data| is correctly encoded according to the specified MIME-type.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CLIPBOARD_STUB_H_
