// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_
#define FUCHSIA_ENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_

#include <lib/fidl/cpp/binding_set.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "fuchsia/engine/browser/context_impl.h"

namespace cr_fuchsia {

// Base test class used for testing the WebEngine Context FIDL service in
// integration.
class WebEngineBrowserTest : public content::BrowserTestBase {
 public:
  WebEngineBrowserTest();
  ~WebEngineBrowserTest() override;

  // Sets the Context client channel which will be bound to a Context FIDL
  // object by WebEngineBrowserTest.
  static void SetContextClientChannel(zx::channel channel);

  // Creates a Frame for this Context.
  // |listener|: If set, specifies the navigation listener for the Frame.
  fuchsia::web::FramePtr CreateFrame(
      fuchsia::web::NavigationEventListener* listener);

  // Gets the client object for the Context service.
  fuchsia::web::ContextPtr& context() { return context_; }

  // Gets the underlying ContextImpl service instance.
  ContextImpl* context_impl() const;

  fidl::BindingSet<fuchsia::web::NavigationEventListener>&
  navigation_listener_bindings() {
    return navigation_listener_bindings_;
  }

  void set_test_server_root(const base::FilePath& path) {
    test_server_root_ = path;
  }

  // content::BrowserTestBase implementation.
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  base::FilePath test_server_root_;
  fuchsia::web::ContextPtr context_;
  fidl::BindingSet<fuchsia::web::NavigationEventListener>
      navigation_listener_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineBrowserTest);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_TEST_WEB_ENGINE_BROWSER_TEST_H_
