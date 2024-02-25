// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_
#define CONTENT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_

#include <fuchsia/element/cpp/fidl.h>

namespace content {

class FuchsiaViewPresenter final {
 public:
  FuchsiaViewPresenter();
  ~FuchsiaViewPresenter();

  FuchsiaViewPresenter(const FuchsiaViewPresenter&) = delete;
  FuchsiaViewPresenter& operator=(const FuchsiaViewPresenter&) = delete;

 private:
  fuchsia::element::ViewControllerPtr PresentFlatlandView(
      fuchsia::ui::views::ViewportCreationToken viewport_creation_token);

  bool callbacks_were_set_ = false;
  fuchsia::element::GraphicalPresenterPtr graphical_presenter_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_
