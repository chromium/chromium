// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/service_directory.h>

#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using FrameHostImplBrowserTest = WebEngineBrowserTest;

}  // namespace

// Verify that it is possible to connect to the fuchsia.web.FrameHost service,
// and that a Frame created in it uses a different ContextImpl than one created
// via the fuchsia.web.Context.
IN_PROC_BROWSER_TEST_F(FrameHostImplBrowserTest, IsolatedFromWebContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a new Frame via the fuchsia.web.Context, and spin the loop to
  // allow the request to be processed.
  fuchsia::web::FramePtr context_frame;
  context()->CreateFrameWithParams({}, context_frame.NewRequest());
  base::RunLoop().RunUntilIdle();

  // Verify that the FrameImpl can be found via the |context_impl()| to which
  // the fuchsia.web.Context is connected.
  EXPECT_TRUE(context_impl()->GetFrameImplForTest(&context_frame) != nullptr);

  // Create a new Frame via a FrameHost instance.
  fuchsia::web::FrameHostPtr frame_host;
  published_services().Connect(frame_host.NewRequest());
  fuchsia::web::FramePtr frame_host_frame;
  frame_host->CreateFrameWithParams({}, frame_host_frame.NewRequest());
  base::RunLoop().RunUntilIdle();

  // Verify that the new Frame cannot be resolved to a FrameImpl under the
  // |context_impl()| to which the fuchsia.web.Context is connected.
  EXPECT_TRUE(context_impl()->GetFrameImplForTest(&frame_host_frame) ==
              nullptr);
}
