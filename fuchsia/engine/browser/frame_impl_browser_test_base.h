// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_

#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Base test class used for testing FrameImpl and the WebEngine Frame FIDL
// service
class FrameImplTestBase : public cr_fuchsia::WebEngineBrowserTest {
 public:
  FrameImplTestBase(const FrameImplTestBase&) = delete;
  FrameImplTestBase& operator=(const FrameImplTestBase&) = delete;

 protected:
  FrameImplTestBase();
  ~FrameImplTestBase() override = default;

  // Creates a Frame without a navigation listener attached.
  virtual fuchsia::web::FramePtr CreateFrame();
};

// Base test class used for testing FrameImpl and the WebEngine Frame FIDL
// service when loading URLs from a test server.
class FrameImplTestBaseWithServer : public FrameImplTestBase {
 public:
  FrameImplTestBaseWithServer(const FrameImplTestBaseWithServer&) = delete;
  FrameImplTestBaseWithServer& operator=(const FrameImplTestBaseWithServer&) =
      delete;

  void SetUpOnMainThread() override;

 protected:
  FrameImplTestBaseWithServer();
  ~FrameImplTestBaseWithServer() override = default;

  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() override;

  cr_fuchsia::TestNavigationListener navigation_listener_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_
