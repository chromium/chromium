// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONDITIONAL_COMPOSER_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_CONDITIONAL_COMPOSER_MOUSE_CURSOR_MONITOR_H_

#include <memory>

#include "remoting/host/desktop_and_cursor_conditional_composer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/protocol/client_stub.h"

namespace remoting {

// Interface for a composing capturer manager class. If the implementation on
// a given platform allows creation of the composing desktop capturer, then
// it will also manage the lifecycle of any mouse shape pump monitors that go
// with the capturer. The implementation keeps a pointer to the created
// composers created by this interface and hence the composers created by this
// manager class should be destroyed after this class.
class ComposingCapturerCursorMonitorManager {
 public:
  // |desktop_environment| should outlive instance of this class.
  static std::unique_ptr<ComposingCapturerCursorMonitorManager> Create(
      DesktopEnvironment* desktop_environment);

  virtual ~ComposingCapturerCursorMonitorManager() = default;

  // Creates and returns the composing video capturer if supported by the
  // platform. Returns null if the platform doesn't support composition.
  // If platform supports composition, then mouse pump monitors will be
  // created internally to monitor the mouse cursor and shape for the
  // composer / stream.
  virtual std::unique_ptr<DesktopAndCursorConditionalComposer>
  CreateComposingVideoCapturer(protocol::ClientStub* client_stub) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CONDITIONAL_COMPOSER_MOUSE_CURSOR_MONITOR_H_
