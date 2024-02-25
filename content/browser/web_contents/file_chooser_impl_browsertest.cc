// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
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

  void ClearListener() { listener_ = nullptr; }

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

IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, ListenerOutlivingChooser) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  auto delegate =
      std::make_unique<FileChooserImplBrowserTestWebContentsDelegate>();
  shell()->web_contents()->SetDelegate(delegate.get());

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto chooser_and_remote = FileChooserImpl::CreateForTesting(rfh);
  auto* chooser = chooser_and_remote.first;

  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           base::DoNothing());

  static_cast<FileChooserImpl::FileSelectListenerImpl*>(delegate->listener())
      ->SetListenerFunctionCalledTrueForTesting();
  // After this, `chooser` will no longer have a reference to the listener.
  chooser->FileSelectionCanceled();

  // Destroy `chooser`.
  chooser_and_remote.second.reset();
  // The destruction happens asynchronously after the disconnection.
  base::RunLoop await_chooser_destruction_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, await_chooser_destruction_run_loop.QuitClosure());
  await_chooser_destruction_run_loop.Run();

  // Clear the last reference to the listener, which will destroy it. It
  // should gracefully handle being destroyed after the chooser.
  delegate->ClearListener();
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
  chooser->FileSelected(base::FilePath(),
                        blink::mojom::FileChooserParams::Mode::kOpen,
                        std::move(files));

  // Test passes if this run_loop.Run() returns instead of timing out.
  run_loop.Run();
}

// https://crbug.com/1345275
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, UploadFolderWithSymlink) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl(".", "file_input_webkitdirectory.html")));

  // The folder contains a regular file and a symbolic link.
  // When uploading the folder, the symbolic link should be excluded.
  base::FilePath dir_test_data;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath folder_to_upload =
      dir_test_data.AppendASCII("file_chooser").AppendASCII("dir_with_symlink");

  base::FilePath text_file = folder_to_upload.AppendASCII("text_file.txt");
  base::FilePath symlink_file = folder_to_upload.AppendASCII("symlink");

  // Skip the test if symbolic links are not supported.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::IsLink(symlink_file))
      return;
  }

  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(
      {text_file, symlink_file}, folder_to_upload, base::OnceClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  let listener = new Promise("
                     "      resolve => fileinput.onchange = resolve);"
                     "  fileinput.click();"
                     "  await listener;"
                     "})()"));

  EXPECT_EQ(
      1, EvalJs(shell(), "document.getElementById('fileinput').files.length;"));
  EXPECT_EQ(
      "text_file.txt",
      EvalJs(shell(), "document.getElementById('fileinput').files[0].name;"));
}

// https://crbug.com/1378997
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, UploadFolderWithDirSymlink) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl(".", "file_input_webkitdirectory.html")));

  // The folder contains a regular file and a directory symbolic link.
  // When uploading the folder, the symbolic link should not be followed.
  base::FilePath dir_test_data;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath folder_to_upload = dir_test_data.AppendASCII("file_chooser")
                                        .AppendASCII("dir_with_dir_symlink");

  base::FilePath foo_file = folder_to_upload.AppendASCII("foo.txt");
  base::FilePath dir_symlink = folder_to_upload.AppendASCII("symlink");
  base::FilePath bar_file = dir_symlink.AppendASCII("bar.txt");

  // Skip the test if symbolic links are not supported.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::IsLink(dir_symlink))
      return;
  }

  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(
      {foo_file, bar_file}, folder_to_upload, base::OnceClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  let listener = new Promise("
                     "      resolve => fileinput.onchange = resolve);"
                     "  fileinput.click();"
                     "  await listener;"
                     "})()"));

  EXPECT_EQ(
      1, EvalJs(shell(), "document.getElementById('fileinput').files.length;"));
  EXPECT_EQ(
      "foo.txt",
      EvalJs(shell(), "document.getElementById('fileinput').files[0].name;"));
}

}  // namespace content
