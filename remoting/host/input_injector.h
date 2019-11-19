// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_H_
#define REMOTING_HOST_INPUT_INJECTOR_H_

#include <memory>

#include "base/memory/ref_counted.h"
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

  // Initialises any objects needed to execute events.
  virtual void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_H_
