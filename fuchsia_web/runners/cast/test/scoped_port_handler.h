// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_SCOPED_PORT_HANDLER_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_SCOPED_PORT_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/test/test_future.h"
#include "components/cast/api_bindings/manager.h"
#include "components/cast/message_port/message_port.h"

// RAII helper that registers a port handler with a `cast_api_bindings::Manager`
// upon construction, waits for the connected port, and automatically
// unregisters the port handler upon destruction.
class ScopedPortHandler {
 public:
  ScopedPortHandler(cast_api_bindings::Manager& manager,
                    std::string_view port_name);
  ~ScopedPortHandler();

  ScopedPortHandler(const ScopedPortHandler&) = delete;
  ScopedPortHandler& operator=(const ScopedPortHandler&) = delete;

  // Drains the run loop until the port is connected, and returns it.
  std::unique_ptr<cast_api_bindings::MessagePort> RunUntilPortConnected();

 private:
  const raw_ref<cast_api_bindings::Manager> manager_;
  const std::string port_name_;
  base::test::TestFuture<std::unique_ptr<cast_api_bindings::MessagePort>>
      future_port_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_SCOPED_PORT_HANDLER_H_
