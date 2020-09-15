// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

using FileChooserImplBrowserTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, FileChooserAfterRfhDeath) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  mojo::Remote<blink::mojom::FileChooser> chooser =
      FileChooserImpl::CreateBoundForTesting(rfh);

  // Kill the renderer process.
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  auto quit_run_loop = [](base::RunLoop* run_loop,
                          blink::mojom::FileChooserResultPtr result) {
    run_loop->Quit();
  };

  // Call FileChooser methods.  The browser process should not crash.
  base::RunLoop run_loop1;
  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           base::BindOnce(quit_run_loop, &run_loop1));
  run_loop1.Run();

  base::RunLoop run_loop2;
  chooser->EnumerateChosenDirectory(base::FilePath(),
                                    base::BindOnce(quit_run_loop, &run_loop2));
  run_loop2.Run();

  // Pass if this didn't crash.
}

}  // namespace content
