// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_

#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/test/browser_test_base.h"

class ContextImpl;
class FrameHostImpl;

namespace base {
class CommandLine;
}

namespace sys {
class ServiceDirectory;
}

// Base test class used for testing the WebEngine Context FIDL service in
// integration.
class WebEngineBrowserTest : public content::BrowserTestBase {
 public:
  WebEngineBrowserTest();
  ~WebEngineBrowserTest() override;

  WebEngineBrowserTest(const WebEngineBrowserTest&) = delete;
  WebEngineBrowserTest& operator=(const WebEngineBrowserTest&) = delete;

  // Provides access to the set of services published by this browser process,
  // through its outgoing directory.
  sys::ServiceDirectory& published_services();

  // Gets the client object for the Context service.
  fuchsia::web::ContextPtr& context() { return context_; }

  // Gets the bound ContextImpl instance. Crashes if there is no ContextImpl.
  ContextImpl* context_impl() const;

  // Gets the FrameHostImpl instances that are bound.
  std::vector<FrameHostImpl*> frame_host_impls() const;

  void SetHeadlessInCommandLine(base::CommandLine* command_line);

  void set_test_server_root(const base::FilePath& path) {
    test_server_root_ = path;
  }

  // content::BrowserTestBase implementation.
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 private:
  base::FilePath test_server_root_;
  fuchsia::web::ContextPtr context_;

  // Client for the directory of services published by this browser process.
  std::shared_ptr<sys::ServiceDirectory> published_services_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_
