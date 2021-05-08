// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_FRAME_TEST_HELPER_H_
#define FUCHSIA_ENGINE_TEST_FRAME_TEST_HELPER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "fuchsia/base/test_navigation_listener.h"

namespace cr_fuchsia {

// Helper for tests which need to create fuchsia.web.Frames.
class FrameTestHelper {
 public:
  FrameTestHelper(fuchsia::web::Context* context,
                  fuchsia::web::CreateFrameParams params);
  FrameTestHelper(fuchsia::web::FrameHost* frame_host,
                  fuchsia::web::CreateFrameParams params);
  FrameTestHelper(const fuchsia::web::ContextPtr& context,
                  fuchsia::web::CreateFrameParams params);
  ~FrameTestHelper();

  FrameTestHelper(const FrameTestHelper&) = delete;
  FrameTestHelper& operator=(const FrameTestHelper&) = delete;

  // Returns a new NavigationController for each call, which ensures that any
  // calls made to |frame()| will have been processed before navigation
  // controller requests.
  fuchsia::web::NavigationControllerPtr GetNavigationController();

  fuchsia::web::FramePtr& frame() { return frame_; }
  TestNavigationListener& navigation_listener() { return navigation_listener_; }

 private:
  fuchsia::web::FramePtr frame_;
  TestNavigationListener navigation_listener_;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_TEST_FRAME_TEST_HELPER_H_
