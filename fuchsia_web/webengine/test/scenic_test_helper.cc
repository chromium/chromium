// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/scenic_test_helper.h"

#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

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

ScenicTestHelper::ScenicTestHelper() {}
ScenicTestHelper::~ScenicTestHelper() = default;

// Simulate the creation of a Scenic View, except bypassing the actual
// construction of a ScenicPlatformWindow in favor of using an injected
// StubWindow.
void ScenicTestHelper::CreateScenicView(FrameImpl* frame_impl,
                                        fuchsia::web::FramePtr& frame) {
  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  view_ref_ = std::move(view_ref_pair.view_ref);
  fuchsia::ui::views::ViewRef view_ref_dup;
  zx_status_t status = view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS,
                                                     &view_ref_dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";

  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateViewWithViewRef(std::move(view_tokens.view_token),
                               std::move(view_ref_pair.control_ref),
                               std::move(view_ref_dup));
  base::RunLoop().RunUntilIdle();
  frame_impl->window_tree_host_for_test()->Show();
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
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}
