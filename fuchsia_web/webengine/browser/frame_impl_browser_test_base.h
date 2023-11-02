// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_

#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Base test class used for testing FrameImpl and the WebEngine Frame FIDL
// service
class FrameImplTestBase : public WebEngineBrowserTest {
 public:
  FrameImplTestBase(const FrameImplTestBase&) = delete;
  FrameImplTestBase& operator=(const FrameImplTestBase&) = delete;

 protected:
  FrameImplTestBase();
  ~FrameImplTestBase() override = default;
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

  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_IMPL_BROWSER_TEST_BASE_H_
