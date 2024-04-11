// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/fuchsia_view_presenter.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

namespace content {

FuchsiaViewPresenter::FuchsiaViewPresenter() {
  // The presenter callbacks may be already set when running tests. In that case
  // they don't need to be set again.
  if (ui::fuchsia::GetFlatlandViewPresenter()) {
    return;
  }

  base::ComponentContextForProcess()->svc()->Connect(
      graphical_presenter_.NewRequest());
  graphical_presenter_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia.element.GraphicalPresenter"));

  ui::fuchsia::SetFlatlandViewPresenter(base::BindRepeating(
      &FuchsiaViewPresenter::PresentFlatlandView, base::Unretained(this)));
  callbacks_were_set_ = true;
}

FuchsiaViewPresenter::~FuchsiaViewPresenter() {
  if (callbacks_were_set_) {
    ui::fuchsia::SetFlatlandViewPresenter(base::NullCallback());
  }
}

fuchsia::element::ViewControllerPtr FuchsiaViewPresenter::PresentFlatlandView(
    fuchsia::ui::views::ViewportCreationToken viewport_creation_token) {
  fuchsia::element::ViewControllerPtr view_controller;
  fuchsia::element::ViewSpec view_spec;
  view_spec.set_viewport_creation_token(std::move(viewport_creation_token));
  graphical_presenter_->PresentView(std::move(view_spec), nullptr,
                                    view_controller.NewRequest(),
                                    [](auto result) {});
  return view_controller;
}

}  // namespace content