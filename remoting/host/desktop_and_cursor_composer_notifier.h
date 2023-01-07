// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_NOTIFIER_H_
#define REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_NOTIFIER_H_

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/input_filter.h"

namespace remoting {

// Non-filtering InputStub implementation which detects changes into or out of
// relative pointer mode, which corresponds to client-side pointer lock. Coupled
// with a local input monitor detecting local mouse activity, this can be used
// to enable or disable compositing of the local mouse cursor.
class DesktopAndCursorComposerNotifier : public protocol::InputFilter {
 public:
  class EventHandler {
   public:
    virtual void SetComposeEnabled(bool enabled) = 0;
  };

  DesktopAndCursorComposerNotifier(InputStub* input_stub,
                                   EventHandler* event_handler_);

  DesktopAndCursorComposerNotifier(const DesktopAndCursorComposerNotifier&) =
      delete;
  DesktopAndCursorComposerNotifier& operator=(
      const DesktopAndCursorComposerNotifier&) = delete;

  ~DesktopAndCursorComposerNotifier() override;

  // InputStub overrides.
  void InjectMouseEvent(const protocol::MouseEvent& event) override;

  void OnLocalInput();

 private:
  void NotifyEventHandler(bool enabled);

  raw_ptr<EventHandler> event_handler_;
  bool has_triggered_ = false;
  bool is_enabled_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_NOTIFIER_H_
