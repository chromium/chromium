// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_FAKE_CONTEXT_H_
#define FUCHSIA_ENGINE_FAKE_CONTEXT_H_

#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>

#include <utility>

#include "base/callback.h"
#include "base/macros.h"

// A fake Frame implementation that manages its own lifetime.
class FakeFrame : public fuchsia::web::testing::Frame_TestBase {
 public:
  explicit FakeFrame(fidl::InterfaceRequest<fuchsia::web::Frame> request);
  ~FakeFrame() override;

  void set_on_set_listener_callback(base::OnceClosure callback) {
    on_set_listener_callback_ = std::move(callback);
  }

  // Tests can provide e.g a mock NavigationController, which the FakeFrame will
  // pass bind GetNavigationController() requests to.
  void set_navigation_controller(
      fuchsia::web::NavigationController* controller) {
    navigation_controller_ = controller;
  }

  fuchsia::web::NavigationEventListener* listener() { return listener_.get(); }

  // fuchsia::web::Frame implementation.
  void GetNavigationController(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller)
      override;
  void SetNavigationEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener)
      override;

  // fuchsia::web::testing::Frame_TestBase implementation.
  void NotImplemented_(const std::string& name) override;

 private:
  fidl::Binding<fuchsia::web::Frame> binding_;
  fuchsia::web::NavigationEventListenerPtr listener_;
  base::OnceClosure on_set_listener_callback_;

  fuchsia::web::NavigationController* navigation_controller_ = nullptr;
  fidl::BindingSet<fuchsia::web::NavigationController>
      navigation_controller_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeFrame);
};

// An implementation of Context that creates and binds FakeFrames.
class FakeContext : public fuchsia::web::testing::Context_TestBase {
 public:
  using CreateFrameCallback = base::RepeatingCallback<void(FakeFrame*)>;

  FakeContext();
  ~FakeContext() override;

  // Sets a callback that is invoked whenever new Frames are bound.
  void set_on_create_frame_callback(CreateFrameCallback callback) {
    on_create_frame_callback_ = callback;
  }

  // fuchsia::web::Context implementation.
  void CreateFrame(
      fidl::InterfaceRequest<fuchsia::web::Frame> frame_request) override;

  // fuchsia::web::testing::Context_TestBase implementation.
  void NotImplemented_(const std::string& name) override;

 private:
  CreateFrameCallback on_create_frame_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeContext);
};

#endif  // FUCHSIA_ENGINE_FAKE_CONTEXT_H_
