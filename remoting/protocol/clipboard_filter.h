// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_
#define REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {
namespace protocol {

// Forwards clipboard events to |clipboard_stub|, if configured.  Event
// forwarding may also be disabled independently of the configured
// |clipboard_stub|. ClipboardFilters initially have event forwarding enabled.
class ClipboardFilter : public ClipboardStub {
 public:
  ClipboardFilter();
  explicit ClipboardFilter(ClipboardStub* clipboard_stub);
  ~ClipboardFilter() override;

  // Set the ClipboardStub that events will be forwarded to.
  void set_clipboard_stub(ClipboardStub* clipboard_stub);

  // Enable/disable forwarding of clipboard events to the ClipboardStub.
  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  // ClipboardStub interface.
  void InjectClipboardEvent(const ClipboardEvent& event) override;

 private:
  ClipboardStub* clipboard_stub_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardFilter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_
