// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include <optional>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
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

// A folder picked in the browser's file picker may itself be a symlink, e.g. a
// symlinked home directory, and its contents are uploaded.
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, UploadFolderIsDirSymlink) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl(".", "file_input_webkitdirectory.html")));

  base::FilePath dir_test_data;
  ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath folder_to_upload = dir_test_data.AppendASCII("file_chooser")
                                        .AppendASCII("dir_with_dir_symlink")
                                        .AppendASCII("symlink");

  base::FilePath bar_file = folder_to_upload.AppendASCII("bar.txt");

  // Skip the test if symbolic links are not supported.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::IsLink(folder_to_upload)) {
      GTEST_SKIP() << "Symbolic links are not supported on this platform.";
    }
  }

  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(
      {bar_file}, folder_to_upload, base::OnceClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  let listener = new Promise("
                     "      resolve => fileinput.onchange = resolve);"
                     "  fileinput.click();"
                     "  await listener;"
                     "})()"));

  // The fix for https://crbug.com/497482008 made this upload empty, because
  // RemoveSymlinks() couldn't tell a folder the user picked from one that a
  // compromised renderer nominated via EnumerateChosenDirectory(). That was
  // overly restrictive for the picker. Renderer-nominated folders are now
  // checked up to the user's grant instead (see
  // FileChooserImplEnumerateBrowserTest), so a picked folder is trusted even if
  // it is a symlink, as it was before that fix.
  EXPECT_EQ(
      1, EvalJs(shell(), "document.getElementById('fileinput').files.length;"));
  EXPECT_EQ(
      "bar.txt",
      EvalJs(shell(), "document.getElementById('fileinput').files[0].name;"));
}

#if BUILDFLAG(IS_POSIX)

// Calls EnumerateChosenDirectory() as a renderer would after the user shared
// (e.g. dropped) a folder. Enumerates the real disk, following symlinks.
class FileChooserImplEnumerateBrowserTest : public ContentBrowserTest,
                                            public WebContentsDelegate {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    shell()->web_contents()->SetDelegate(this);
  }

  void TearDownOnMainThread() override {
    shell()->web_contents()->SetDelegate(shell());
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // WebContentsDelegate:
  void EnumerateDirectory(WebContents* web_contents,
                          scoped_refptr<FileSelectListener> listener,
                          const base::FilePath& path) override {
    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FileEnumerator e(path, /*recursive=*/true,
                           base::FileEnumerator::FILES);
    for (base::FilePath file = e.Next(); !file.empty(); file = e.Next()) {
      files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(file, std::u16string(),
                                            std::vector<std::u16string>())));
    }
    listener->FileSelected(
        std::move(files), path,
        blink::mojom::FileChooserParams::Mode::kUploadFolder);
  }

  base::FilePath Path(std::string_view relative) {
    return temp_dir_.GetPath().Append(relative);
  }

  void WriteFile(std::string_view relative) {
    ASSERT_TRUE(base::CreateDirectory(Path(relative).DirName()));
    ASSERT_TRUE(base::WriteFile(Path(relative), "x"));
  }

  void Symlink(std::string_view target, std::string_view link) {
    ASSERT_TRUE(base::CreateSymbolicLink(Path(target), Path(link)));
  }

  ChildProcessId process_id() {
    return shell()
        ->web_contents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->GetID();
  }

  void Share(std::string_view relative) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        process_id(), Path(relative));
  }

  // Returns the files that the renderer gets back, or nullopt if the request
  // was rejected.
  std::optional<std::vector<base::FilePath>> Enumerate(
      std::string_view relative) {
    auto remote = FileChooserImpl::CreateBoundForTesting(
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetPrimaryMainFrame()));
    base::test::TestFuture<blink::mojom::FileChooserResultPtr> future;
    remote->EnumerateChosenDirectory(Path(relative), future.GetCallback());
    blink::mojom::FileChooserResultPtr chooser_result = future.Take();

    // Let the chooser be destroyed, so it's no longer the active chooser.
    remote.reset();
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    if (!chooser_result) {
      return std::nullopt;
    }
    std::vector<base::FilePath> result;
    for (const auto& file : chooser_result->files) {
      result.push_back(file->get_native_file()->file_path);
    }
    return result;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileChooserImplEnumerateBrowserTest, NoSymlinks) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("shared/a.txt");
  WriteFile("shared/sub/b.txt");
  WriteFile("other/c.txt");
  Share("shared");

  EXPECT_THAT(Enumerate("shared"),
              testing::Optional(testing::UnorderedElementsAre(
                  Path("shared/a.txt"), Path("shared/sub/b.txt"))));
  EXPECT_THAT(
      Enumerate("shared/sub"),
      testing::Optional(testing::ElementsAre(Path("shared/sub/b.txt"))));
  EXPECT_EQ(Enumerate("other"), std::nullopt);
}

// A renderer may nominate a directory symlink inside the folder the user
// shared. The symlink points outside the shared folder, so nothing in it is
// returned. https://crbug.com/497482008
IN_PROC_BROWSER_TEST_F(FileChooserImplEnumerateBrowserTest,
                       NominatedDirIsSymlink) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("outside/secret.txt");
  ASSERT_TRUE(base::CreateDirectory(Path("shared")));
  Symlink("outside", "shared/link");
  Share("shared");

  EXPECT_THAT(Enumerate("shared/link"), testing::Optional(testing::IsEmpty()));
}

// A renderer may nominate a real directory that is reached through a symlink
// inside the shared folder. The symlink check covers every directory between
// the shared folder and each file, not just those below the nominated
// directory, so nothing is returned. https://crbug.com/564459472
IN_PROC_BROWSER_TEST_F(FileChooserImplEnumerateBrowserTest,
                       SymlinkAboveNominatedDir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("outside/sub/secret.txt");
  ASSERT_TRUE(base::CreateDirectory(Path("shared")));
  Symlink("outside", "shared/link");
  Share("shared");
  // The lexical check lets the renderer nominate this path.
  ASSERT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id(), Path("shared/link/sub")));

  EXPECT_THAT(Enumerate("shared/link/sub"),
              testing::Optional(testing::IsEmpty()));
}

// The shared folder may itself be a symlink, e.g. a symlinked home directory,
// while symlinks inside it are still excluded.
IN_PROC_BROWSER_TEST_F(FileChooserImplEnumerateBrowserTest,
                       SharedDirIsSymlink) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("real/a.txt");
  WriteFile("real/sub/b.txt");
  WriteFile("outside/sub/secret.txt");
  Symlink("outside", "real/link");
  Symlink("real", "shared");
  Share("shared");

  EXPECT_THAT(Enumerate("shared"),
              testing::Optional(testing::UnorderedElementsAre(
                  Path("shared/a.txt"), Path("shared/sub/b.txt"))));
  EXPECT_THAT(
      Enumerate("shared/sub"),
      testing::Optional(testing::ElementsAre(Path("shared/sub/b.txt"))));
  EXPECT_THAT(Enumerate("shared/link/sub"),
              testing::Optional(testing::IsEmpty()));
}

// The nearest shared ancestor bounds the check, not the topmost one.
IN_PROC_BROWSER_TEST_F(FileChooserImplEnumerateBrowserTest, NestedShares) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile("root/real/photos/p.jpg");
  Symlink("root/real", "root/link");
  Share("root");
  Share("root/link/photos");

  EXPECT_THAT(
      Enumerate("root/link/photos"),
      testing::Optional(testing::ElementsAre(Path("root/link/photos/p.jpg"))));
  EXPECT_THAT(Enumerate("root/link"), testing::Optional(testing::IsEmpty()));
}

#endif  // BUILDFLAG(IS_POSIX)

// Ensure that FileChooserImpl::FileSelected does not grant permissions if
// invoked with Mode::kSave, as a defense against compromised renderers.
// See https://crbug.com/435684924.
IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest,
                       FileSelectedWithSaveModeDoesNotGrantPermission) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto chooser_and_remote = FileChooserImpl::CreateForTesting(rfh);
  auto* chooser = chooser_and_remote.first;

  base::FilePath test_file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &test_file));
  test_file = test_file.AppendASCII("sensitive_file.txt");

  // Ensure renderer doesn't have access initially.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_FALSE(policy->CanReadFile(rfh->GetProcess()->GetID(), test_file));

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  files.emplace_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
      blink::mojom::NativeFileInfo::New(test_file, std::u16string(),
                                        std::vector<std::u16string>())));

  // Call FileSelected with kSave.
  chooser->FileSelected(base::FilePath(),
                        blink::mojom::FileChooserParams::Mode::kSave,
                        std::move(files));

  // Verify renderer STILL doesn't have access.
  EXPECT_FALSE(policy->CanReadFile(rfh->GetProcess()->GetID(), test_file));
}

#if BUILDFLAG(IS_ANDROID)
class DisallowSystemUiPopupsContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool ShouldAllowSystemUiPopups(WebContents* web_contents) override {
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(FileChooserImplBrowserTest, DisallowSystemUiPopups) {
  DisallowSystemUiPopupsContentBrowserClient test_client;
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto chooser_and_remote = FileChooserImpl::CreateForTesting(rfh);
  auto* chooser = chooser_and_remote.first;

  base::test::TestFuture<blink::mojom::FileChooserResultPtr> future;
  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           future.GetCallback());
  EXPECT_TRUE(future.Get().is_null());
}
#endif

}  // namespace content
