// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/pasteboard_changed_observation.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

class ClipboardMac : public Clipboard {
 public:
  ClipboardMac();

  ClipboardMac(const ClipboardMac&) = delete;
  ClipboardMac& operator=(const ClipboardMac&) = delete;

  ~ClipboardMac() override;

  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  void ClipboardChanged();

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;
  base::CallbackListSubscription clipboard_change_subscription_;
  NSInteger current_change_count_ = 0;
};

ClipboardMac::ClipboardMac() = default;

ClipboardMac::~ClipboardMac() = default;

void ClipboardMac::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_ = std::move(client_clipboard);
  current_change_count_ = NSPasteboard.generalPasteboard.changeCount;

  // Unretained is safe because the subscription's lifetime is scoped to the
  // lifetime of this object.
  clipboard_change_subscription_ =
      base::RegisterPasteboardChangedCallback(base::BindRepeating(
          &ClipboardMac::ClipboardChanged, base::Unretained(this)));
}

void ClipboardMac::InjectClipboardEvent(const protocol::ClipboardEvent& event) {
  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0) {
    return;
  }
  if (!base::IsStringUTF8AllowingNoncharacters(event.data())) {
    LOG(ERROR) << "ClipboardEvent data is not UTF-8 encoded.";
    return;
  }

  // Write text to clipboard.
  NSString* text = base::SysUTF8ToNSString(event.data());
  NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
  [pasteboard clearContents];
  [pasteboard writeObjects:@[ text ]];

  // Update local change-count to prevent this change from being picked up by
  // ClipboardChanged().
  current_change_count_ = NSPasteboard.generalPasteboard.changeCount;
}

void ClipboardMac::ClipboardChanged() {
  NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
  NSInteger change_count = pasteboard.changeCount;
  if (change_count == current_change_count_) {
    return;
  }

  NSArray* objects = [pasteboard readObjectsForClasses:@[ [NSString class] ]
                                               options:nil];
  if (!objects.count) {
    return;
  }

  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data(base::SysNSStringToUTF8(objects.lastObject));
  client_clipboard_->InjectClipboardEvent(event);
}

std::unique_ptr<Clipboard> Clipboard::Create() {
  return base::WrapUnique(new ClipboardMac());
}

}  // namespace remoting
