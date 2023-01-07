// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_V1_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_V1_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/command_line.h"

// Starts a WebEngine and connects a ContextProvider instance for tests.
// WebEngine logs will be included in the test output but not in the Fuchsia
// system log.
class ContextProviderForTest {
 public:
  static ContextProviderForTest Create(const base::CommandLine& command_line);

  ContextProviderForTest(ContextProviderForTest&&) noexcept;
  ContextProviderForTest& operator=(ContextProviderForTest&&) noexcept;
  ~ContextProviderForTest();

  ::fuchsia::web::ContextProviderPtr& ptr() { return context_provider_; }
  ::fuchsia::web::ContextProvider* get() { return context_provider_.get(); }
  ::fuchsia::sys::ComponentControllerPtr& component_controller_ptr() {
    return web_engine_controller_;
  }

 private:
  ContextProviderForTest(
      ::fuchsia::sys::ComponentControllerPtr web_engine_controller,
      ::fuchsia::web::ContextProviderPtr context_provider);

  ::fuchsia::sys::ComponentControllerPtr web_engine_controller_;
  ::fuchsia::web::ContextProviderPtr context_provider_;
};

// As ContextProviderForTest, but additionally provides access to the
// WebEngine's fuchsia::web::Debug interface.
class ContextProviderForDebugTest {
 public:
  static ContextProviderForDebugTest Create(
      const base::CommandLine& command_line);

  ContextProviderForDebugTest(ContextProviderForDebugTest&&) noexcept;
  ContextProviderForDebugTest& operator=(
      ContextProviderForDebugTest&&) noexcept;
  ~ContextProviderForDebugTest();

  ::fuchsia::web::ContextProviderPtr& ptr() { return context_provider_.ptr(); }
  ::fuchsia::web::ContextProvider* get() { return context_provider_.get(); }

  void ConnectToDebug(
      ::fidl::InterfaceRequest<::fuchsia::web::Debug> debug_request);

 private:
  ContextProviderForDebugTest(ContextProviderForTest context_provider,
                              ::sys::ServiceDirectory debug_service_directory);

  ContextProviderForTest context_provider_;
  ::sys::ServiceDirectory debug_service_directory_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_V1_H_
