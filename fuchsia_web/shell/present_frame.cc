// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/shell/present_frame.h"

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/ui/scenic/cpp/view_creation_tokens.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
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

absl::optional<fuchsia::element::GraphicalPresenterPtr> PresentFrame(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<fuchsia::element::AnnotationController>
        annotation_controller) {
  // TODO(http://crbug.com/1418433): Remove Scenic connection when Flatland
  // lands.
  fuchsia::ui::scenic::ScenicSyncPtr scenic_svc;
  zx_status_t status = base::ComponentContextForProcess()->svc()->Connect(
      scenic_svc.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Couldn't connect to Scenic.";
  bool uses_flatland = false;
  status = scenic_svc->UsesFlatland(&uses_flatland);
  ZX_CHECK(status == ZX_OK, status) << "UsesFlatland()";

  if (uses_flatland) {
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
    scenic::ViewCreationTokenPair token_pair =
        scenic::ViewCreationTokenPair::New();
    fuchsia::element::ViewSpec view_spec;
    view_spec.set_viewport_creation_token(std::move(token_pair.viewport_token));
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
    create_view_args.set_view_creation_token(std::move(token_pair.view_token));
    frame->CreateView2(std::move(create_view_args));

    return presenter;
  }

  // TODO(http://crbug.com/1418433): Remove GFX-specific logic below this
  // comment when Flatland lands.

  // Sync connect to fuchsia.element.GraphicalPresenter so that we can fallback
  // to fuchsia.ui.policy.Presenter if unavailable.
  // TODO(http://crbug.com/1402457): Replace with async when removing fallback.
  fuchsia::element::GraphicalPresenterSyncPtr presenter;
  status = base::ComponentContextForProcess()->svc()->Connect(
      presenter.NewRequest());
  ZX_CHECK(status == ZX_OK, status)
      << "Couldn't connect to GraphicalPresenter.";

  // Generate GFX tokens and frame->CreateViewWithViewRef().
  auto view_tokens = scenic::ViewTokenPair::New();
  auto view_ref_pair = scenic::ViewRefPair::New();

  fuchsia::element::ViewSpec view_spec;
  view_spec.set_view_holder_token(std::move(view_tokens.view_holder_token));
  view_spec.set_view_ref(CloneViewRef(view_ref_pair.view_ref));
  view_spec.set_annotations({});

  fuchsia::element::ViewControllerSyncPtr view_controller;
  fuchsia::element::GraphicalPresenter_PresentView_Result present_view_result;
  status = presenter->PresentView(
      std::move(view_spec), std::move(annotation_controller),
      view_controller.NewRequest(), &present_view_result);

  // Note: We do not consider `present_view_result.is_err()` in the fallback
  // condition in case the FIDL call succeeds but the method reports an error.
  // This is because the only error type reported by the PresentView method is
  // INVALID_ARGS, which we have carefully avoided by:
  // * Providing a view_spec.view_holder_token and view_spec.view_ref (GFX)
  // * Not providing _both_ GFX Views and Flatland Views at once.
  //
  // Therefore, we expect that if the FIDL call succeeds, the presentation
  // should also succeed.
  if (status == ZX_OK) {
    DCHECK(!present_view_result.is_err())
        << "PresentView failed to display the view, reason: "
        << static_cast<uint32_t>(present_view_result.err());
  } else {
    // Fallback to connect to Root Presenter.
    // TODO(http://crbug.com/1402457): Remove fallback.
    LOG(INFO) << "PresentView failed to connect, reason: " << status
              << ". Falling back to fuchsia.ui.policy.Presenter.";
    auto root_presenter = base::ComponentContextForProcess()
                              ->svc()
                              ->Connect<fuchsia::ui::policy::Presenter>();

    // Replace the original ViewToken and ViewRefPair with new ones.
    view_tokens = scenic::ViewTokenPair::New();
    view_ref_pair = scenic::ViewRefPair::New();

    root_presenter->PresentOrReplaceView2(
        std::move(view_tokens.view_holder_token),
        CloneViewRef(view_ref_pair.view_ref), nullptr);
  }

  // Present a fullscreen view of |frame|.
  frame->CreateViewWithViewRef(std::move(view_tokens.view_token),
                               std::move(view_ref_pair.control_ref),
                               std::move(view_ref_pair.view_ref));

  return absl::nullopt;
}
