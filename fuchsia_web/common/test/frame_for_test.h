// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_FRAME_FOR_TEST_H_
#define FUCHSIA_WEB_COMMON_TEST_FRAME_FOR_TEST_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

class TestNavigationListener;

// Helper for tests which need to create fuchsia.web.Frames.
// Each instance owns a fuchsia.web.Frame, and attaches a TestNavigationListener
// to it.
class FrameForTest {
 public:
  // Returns a FrameForTest that encapsulates a new Frame, created using the
  // specified container and |params|.
  static FrameForTest Create(fuchsia::web::Context* context,
                             fuchsia::web::CreateFrameParams params);
  static FrameForTest Create(fuchsia::web::FrameHost* frame_host,
                             fuchsia::web::CreateFrameParams params);
  static FrameForTest Create(const fuchsia::web::ContextPtr& context,
                             fuchsia::web::CreateFrameParams params);
  static FrameForTest Create(const fuchsia::web::FrameHostPtr& frame_host,
                             fuchsia::web::CreateFrameParams params);

  FrameForTest();
  FrameForTest(FrameForTest&&);
  FrameForTest& operator=(FrameForTest&&);
  ~FrameForTest();

  // Initializes navigation_listener() with the specified |flags|. Called in the
  // constructor with empty flags, but tests can call it again to re-initialize
  // with a different set of |flags|.
  void CreateAndAttachNavigationListener(
      fuchsia::web::NavigationEventListenerFlags flags);

  // Returns a new NavigationController for each call, which ensures that any
  // calls made to |frame()| will have been processed before navigation
  // controller requests.
  fuchsia::web::NavigationControllerPtr GetNavigationController();

  // Returns the fuchsia.web.FramePtr owned by this instance.
  fuchsia::web::FramePtr& ptr() { return frame_; }

  // Provide member-dereference operator to improve test readability by letting
  // Frame calls be expressed directly on |this|, rather than via |ptr()|.
  fuchsia::web::Frame* operator->() { return frame_.get(); }

  fuchsia::web::Frame* get() { return frame_.get(); }

  // May be called only on non-default-initialized instances, i.e. those
  // returned directly, or via move-assignment, from Create().
  TestNavigationListener& navigation_listener() {
    return *navigation_listener_;
  }
  fidl::Binding<fuchsia::web::NavigationEventListener>&
  navigation_listener_binding() {
    return *navigation_listener_binding_;
  }

 private:
  fuchsia::web::FramePtr frame_;
  std::unique_ptr<TestNavigationListener> navigation_listener_;
  std::unique_ptr<fidl::Binding<fuchsia::web::NavigationEventListener>>
      navigation_listener_binding_;
};

#endif  // FUCHSIA_WEB_COMMON_TEST_FRAME_FOR_TEST_H_
