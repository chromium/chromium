// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_
#define REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_

#include <optional>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting::protocol {

// Forwards clipboard events to |clipboard_stub|, if configured.  Event
// forwarding may also be disabled independently of the configured
// |clipboard_stub|. ClipboardFilters initially have event forwarding enabled
// and no maximum size set.  If |max_size| is configured, it will be used to
// limit the amount of data transferred when InjectClipboardEvent() is called.
class ClipboardFilter : public ClipboardStub {
 public:
  ClipboardFilter();
  explicit ClipboardFilter(ClipboardStub* clipboard_stub);

  ClipboardFilter(const ClipboardFilter&) = delete;
  ClipboardFilter& operator=(const ClipboardFilter&) = delete;

  ~ClipboardFilter() override;

  // Set the ClipboardStub that events will be forwarded to.
  void set_clipboard_stub(ClipboardStub* clipboard_stub);

  // Enable/disable forwarding of clipboard events to the ClipboardStub.
  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  void set_max_size(size_t max_size) { max_size_ = max_size; }

  // ClipboardStub interface.
  void InjectClipboardEvent(const ClipboardEvent& event) override;

 private:
  raw_ptr<ClipboardStub, DanglingUntriaged> clipboard_stub_ = nullptr;
  bool enabled_ = true;
  std::optional<size_t> max_size_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CLIPBOARD_FILTER_H_
