// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/scenic_test_helper.h"

#include <fuchsia/ui/views/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "content/public/browser/render_widget_host_view.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_window_tree_host.h"
#include "fuchsia_web/webengine/test/test_data.h"

namespace {
const gfx::Rect kBounds = {1000, 1000};
}  // namespace

ScenicTestHelper::ScenicTestHelper() = default;
ScenicTestHelper::~ScenicTestHelper() = default;

// Simulate the creation of a Scenic View, except bypassing the actual
// construction of a ScenicPlatformWindow in favor of using an injected
// StubWindow.
void ScenicTestHelper::CreateScenicView(FrameImpl* frame_impl,
                                        fuchsia::web::FramePtr& frame) {
  DCHECK(frame_impl);
  frame_impl_ = frame_impl;

  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));

  base::RunLoop().RunUntilIdle();
  frame_impl_->window_tree_host_for_test()->Show();
}

void ScenicTestHelper::SetUpViewForInteraction(
    content::WebContents* web_contents) {
  content::RenderWidgetHostView* view =
      web_contents->GetPrimaryMainFrame()->GetView();
  view->SetBounds(kBounds);
  view->Focus();
  base::RunLoop().RunUntilIdle();
}

fuchsia::ui::views::ViewRef ScenicTestHelper::CloneViewRef() {
  DCHECK(frame_impl_);
  return frame_impl_->window_tree_host_for_test()->CreateViewRef();
}
