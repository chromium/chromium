// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/shell/present_frame.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <stdint.h>
#include <zircon/rights.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"

fuchsia::ui::views::ViewRef CloneViewRef(
    const fuchsia::ui::views::ViewRef& view_ref) {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

fuchsia::element::GraphicalPresenterPtr PresentFrame(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<fuchsia::element::AnnotationController>
        annotation_controller) {
  // fuchsia.element.GraphicalPresenter is the only view presentation protocol
  // that supports Flatland, so we expect it to be provided.
  //
  // Note also that using a sync connection results in a 3-way deadlock
  // between web_engine, (Flatland) scene_manager, and scenic, so we use an
  // async connection for the Flatland branch.
  fuchsia::element::GraphicalPresenterPtr presenter =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::element::GraphicalPresenter>();
  presenter.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "GraphicalPresenter disconnected: ";
  });

  // Generate Flatland tokens and frame->CreateView2().
  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  fuchsia::element::ViewSpec view_spec;
  view_spec.set_viewport_creation_token(std::move(viewport_token));
  view_spec.set_annotations({});

  fuchsia::element::ViewControllerPtr view_controller;
  presenter->PresentView(
      std::move(view_spec), std::move(annotation_controller),
      view_controller.NewRequest(),
      [](fuchsia::element::GraphicalPresenter_PresentView_Result result) {
        if (result.is_err()) {
          LOG(ERROR) << "PresentView failed to display the view, reason: "
                     << static_cast<uint32_t>(result.err());
        }
      });

  // Present a fullscreen view of |frame|.
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));

  return presenter;
}
