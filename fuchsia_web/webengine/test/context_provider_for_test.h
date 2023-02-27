// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>

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
  ::component_testing::RealmRoot& realm_root() { return realm_root_; }

 private:
  ContextProviderForTest(::component_testing::RealmRoot realm_root,
                         ::fuchsia::web::ContextProviderPtr context_provider);

  ::component_testing::RealmRoot realm_root_;
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
  explicit ContextProviderForDebugTest(ContextProviderForTest context_provider);

  ContextProviderForTest context_provider_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_CONTEXT_PROVIDER_FOR_TEST_H_
