// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_H_
#define REMOTING_HOST_INPUT_INJECTOR_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/input_injector_metadata.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/input_stub.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// TODO(sergeyu): Move ClipboardStub implementation to Clipboard.
class InputInjector : public protocol::ClipboardStub,
                      public protocol::InputStub {
 public:
  // Creates a default input injector for the current platform. This
  // object should do as much work as possible on |main_task_runner|,
  // using |ui_task_runner| only for tasks actually requiring a UI
  // thread.
  static std::unique_ptr<InputInjector> Create(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Returns true if the InputInjector returned by Create() supports
  // InjectTouchEvent() on this platform.
  static bool SupportsTouchEvents();

  // Initializes any objects needed to execute events.
  virtual void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) = 0;

  // Sets metadata that may be required by the injector to work properly.
  // Currently used by wayland input injector to get the portal session related
  // details from capturer.
  virtual void SetMetadata(InputInjectorMetadata metadata) {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_H_
