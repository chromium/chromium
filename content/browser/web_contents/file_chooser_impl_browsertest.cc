// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_delegate.h"
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
      shell()->web_contents()->GetPrimaryMainFrame());
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

class FileChooserImplBrowserTestWebContentsDelegate
    : public WebContentsDelegate {
 public:
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      scoped_refptr<FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    listener_ = listener;
  }

  FileSelectListener* listener() { return listener_.get(); }

 private:
  scoped_refptr<FileSelectListener> listener_;
};

// This test makes sure that if we kill the renderer after opening the file
// picker, then the cancelation callback is still called in order to prevent
// the parent renderer from thinking that the file picker is still open in the
// case of an iframe.
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest,
                       FileChooserCallbackAfterRfhDeathCancel) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  auto delegate =
      std::make_unique<FileChooserImplBrowserTestWebContentsDelegate>();

  shell()->web_contents()->SetDelegate(delegate.get());

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto chooser_and_remote = FileChooserImpl::CreateForTesting(rfh);
  auto* chooser = chooser_and_remote.first;

  auto quit_run_loop = [](base::RunLoop* run_loop,
                          blink::mojom::FileChooserResultPtr result) {
    run_loop->Quit();
  };

  base::RunLoop run_loop;
  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           base::BindOnce(quit_run_loop, &run_loop));

  // Kill the renderer process
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  static_cast<FileChooserImpl::FileSelectListenerImpl*>(delegate->listener())
      ->SetListenerFunctionCalledTrueForTesting();
  chooser->FileSelectionCanceled();

  // Test passes if this run_loop.Run() returns instead of timing out.
  run_loop.Run();
}

// Same as FileChooserCallbackAfterRfhDeathCancel but with a file selected from
// the file picker.
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest,
                       FileChooserCallbackAfterRfhDeathSelected) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  auto delegate =
      std::make_unique<FileChooserImplBrowserTestWebContentsDelegate>();

  shell()->web_contents()->SetDelegate(delegate.get());

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto chooser_and_remote = FileChooserImpl::CreateForTesting(rfh);
  auto* chooser = chooser_and_remote.first;

  auto quit_run_loop = [](base::RunLoop* run_loop,
                          blink::mojom::FileChooserResultPtr result) {
    run_loop->Quit();
  };

  base::RunLoop run_loop;
  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           base::BindOnce(quit_run_loop, &run_loop));

  // Kill the renderer process
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  static_cast<FileChooserImpl::FileSelectListenerImpl*>(delegate->listener())
      ->SetListenerFunctionCalledTrueForTesting();
  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  files.emplace_back(blink::mojom::FileChooserFileInfoPtr(nullptr));
  chooser->FileSelected(std::move(files), base::FilePath(),
                        blink::mojom::FileChooserParams::Mode::kOpen);

  // Test passes if this run_loop.Run() returns instead of timing out.
  run_loop.Run();
}

}  // namespace content
