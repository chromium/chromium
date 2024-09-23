// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/save_package.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

const char kTestFile[] = "/simple_page.html";

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  explicit TestShellDownloadManagerDelegate(SavePageType save_page_type)
      : save_page_type_(save_page_type) {}

  void ChooseSavePath(WebContents* web_contents,
                      const base::FilePath& suggested_path,
                      const base::FilePath::StringType& default_extension,
                      bool can_save_as_complete,
                      SavePackagePathPickedCallback callback) override {
    content::SavePackagePathPickedParams params;
    params.file_path = suggested_path;
    params.save_type = save_page_type_;
    std::move(callback).Run(std::move(params),
                            SavePackageDownloadCreatedCallback());
  }

  void GetSaveDir(BrowserContext* context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir) override {
    *website_save_dir = download_dir_;
    *download_save_dir = download_dir_;
  }

  bool ShouldCompleteDownload(download::DownloadItem* download,
                              base::OnceClosure closure) override {
    return true;
  }

  base::FilePath download_dir_;
  SavePageType save_page_type_;
};

class DownloadicidalObserver : public DownloadManager::Observer {
 public:
  DownloadicidalObserver(bool remove_download, base::OnceClosure after_closure)
      : remove_download_(remove_download),
        after_closure_(std::move(after_closure)) {}
  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](bool remove_download, base::OnceClosure closure,
                          download::DownloadItem* item) {
                         remove_download ? item->Remove() : item->Cancel(true);
                         std::move(closure).Run();
                       },
                       remove_download_, std::move(after_closure_), item));
  }

 private:
  bool remove_download_;
  base::OnceClosure after_closure_;
};

class DownloadCompleteObserver : public DownloadManager::Observer {
 public:
  explicit DownloadCompleteObserver(base::OnceClosure completed_closure)
      : completed_closure_(std::move(completed_closure)) {}
  DownloadCompleteObserver(const DownloadCompleteObserver&) = delete;
  DownloadCompleteObserver& operator=(const DownloadCompleteObserver&) = delete;

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    DCHECK(!item_observer_);
    mime_type_ = item->GetMimeType();
    target_file_path_ = item->GetTargetFilePath();
    item_observer_ = std::make_unique<DownloadItemCompleteObserver>(
        item, std::move(completed_closure_));
  }

  const std::string& mime_type() const { return mime_type_; }
  const base::FilePath& target_file_path() const { return target_file_path_; }

 private:
  class DownloadItemCompleteObserver : public download::DownloadItem::Observer {
   public:
    DownloadItemCompleteObserver(download::DownloadItem* item,
                                 base::OnceClosure completed_closure)
        : item_(item), completed_closure_(std::move(completed_closure)) {
      item_->AddObserver(this);
    }
    DownloadItemCompleteObserver(const DownloadItemCompleteObserver&) = delete;
    DownloadItemCompleteObserver& operator=(
        const DownloadItemCompleteObserver&) = delete;

    ~DownloadItemCompleteObserver() override {
      if (item_)
        item_->RemoveObserver(this);
    }

   private:
    void OnDownloadUpdated(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      if (item_->GetState() == download::DownloadItem::COMPLETE)
        std::move(completed_closure_).Run();
    }

    void OnDownloadDestroyed(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      item_->RemoveObserver(this);
      item_ = nullptr;
    }

    raw_ptr<download::DownloadItem> item_;
    base::OnceClosure completed_closure_;
  };

  std::unique_ptr<DownloadItemCompleteObserver> item_observer_;
  base::OnceClosure completed_closure_;
  std::string mime_type_;
  base::FilePath target_file_path_;
};

}  // namespace

class SavePackageBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir) {
    *full_file_name = save_dir_.GetPath().AppendASCII(prefix + ".htm");
    *dir = save_dir_.GetPath().AppendASCII(prefix + "_files");
  }

  // Start a SavePackage download and then cancels it. If |remove_download| is
  // true, the download item will be removed while page is being saved.
  // Otherwise, the download item will be canceled.
  void RunAndCancelSavePackageDownload(SavePageType save_page_type,
                                       bool remove_download) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
    EXPECT_TRUE(NavigateToURL(shell(), url));
    auto* download_manager = static_cast<DownloadManagerImpl*>(
        shell()->web_contents()->GetBrowserContext()->GetDownloadManager());
    auto delegate =
        std::make_unique<TestShellDownloadManagerDelegate>(save_page_type);
    delegate->download_dir_ = save_dir_.GetPath();
    auto* old_delegate = download_manager->GetDelegate();
    download_manager->SetDelegate(delegate.get());

    {
      base::RunLoop run_loop;
      DownloadicidalObserver download_item_killer(remove_download,
                                                  run_loop.QuitClosure());
      download_manager->AddObserver(&download_item_killer);

      scoped_refptr<SavePackage> save_package(
          new SavePackage(web_contents_impl()->GetPrimaryPage()));
      save_package->GetSaveInfo();
      run_loop.Run();
      download_manager->RemoveObserver(&download_item_killer);
      EXPECT_TRUE(save_package->canceled());
    }

    // Run a second download to completion so that any pending tasks will get
    // flushed out. If the previous SavePackage operation didn't cleanup after
    // itself, then there could be stray tasks that invoke the now defunct
    // download item.
    {
      base::RunLoop run_loop;
      SavePackageFinishedObserver finished_observer(download_manager,
                                                    run_loop.QuitClosure());
      shell()->web_contents()->OnSavePage();
      run_loop.Run();
    }
    download_manager->SetDelegate(old_delegate);
  }

  WebContentsImpl* web_contents_impl() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;
};

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ImplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(
      new SavePackage(web_contents_impl()->GetPrimaryPage(),
                      SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(
      new SavePackage(web_contents_impl()->GetPrimaryPage(),
                      SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
  save_package->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemDestroyed) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_COMPLETE_HTML, true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemCanceled) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_MHTML, false);
}

// Create a SavePackage and reload the page. This tests that when the
// Reload destroys the primary Page SavePackage's ContinueSaveInfo
// will not crash with a destroyed Page reference.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, Reload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);

  auto* download_manager = static_cast<DownloadManagerImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetDownloadManager());
  auto delegate = std::make_unique<TestShellDownloadManagerDelegate>(
      SAVE_PAGE_TYPE_AS_ONLY_HTML);
  delegate->download_dir_ = save_dir_.GetPath();
  auto* old_delegate = download_manager->GetDelegate();
  download_manager->SetDelegate(delegate.get());

  scoped_refptr<SavePackage> save_package(
      new SavePackage(web_contents_impl()->GetPrimaryPage(),
                      SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
  save_package->GetSaveInfo();
  shell()->web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                                  false /* check_for_repost */);
  download_manager->SetDelegate(old_delegate);
}

class SavePackageFencedFrameBrowserTest : public SavePackageBrowserTest {
 public:
  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  test::FencedFrameTestHelper fenced_frame_helper_;
};

// If fenced frames become savable, this test will need to be updated.
// See https://crbug.com/1321102
IN_PROC_BROWSER_TEST_F(SavePackageFencedFrameBrowserTest,
                       IgnoreFencedFrameInMHTML) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();

  // Create an iframe.
  GURL iframe_url = embedded_test_server()->GetURL("/title2.html");
  constexpr char kAddIframeScript[] = R"({
      (()=>{
          return new Promise((resolve) => {
            const frame = document.createElement('iframe');
            frame.addEventListener('load', () => {resolve();});
            frame.src = $1;
            document.body.appendChild(frame);
          });
      })();
    })";
  EXPECT_TRUE(ExecJs(main_frame, JsReplace(kAddIframeScript, iframe_url)));

  // Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  auto* download_manager = static_cast<DownloadManagerImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetDownloadManager());
  auto delegate = std::make_unique<TestShellDownloadManagerDelegate>(
      SAVE_PAGE_TYPE_AS_MHTML);
  delegate->download_dir_ = save_dir_.GetPath();
  auto* old_delegate = download_manager->GetDelegate();
  download_manager->SetDelegate(delegate.get());

  // Save a page as the MHTML.
  base::FilePath file_path;
  {
    base::RunLoop run_loop;
    DownloadCompleteObserver observer(run_loop.QuitClosure());
    download_manager->AddObserver(&observer);
    scoped_refptr<SavePackage> save_package(
        new SavePackage(web_contents_impl()->GetPrimaryPage()));
    save_package->GetSaveInfo();
    run_loop.Run();
    download_manager->RemoveObserver(&observer);
    EXPECT_TRUE(save_package->finished());
    file_path = observer.target_file_path();
  }

  download_manager->SetDelegate(old_delegate);

  // Read the saved MHTML.
  std::string mhtml;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(file_path, &mhtml));
  }

  // Verify a title in the iframe's document.
  EXPECT_THAT(mhtml, testing::HasSubstr("Title Of Awesomeness"));

  // Verify the absence of the fenced frame's document.
  EXPECT_THAT(mhtml,
              ::testing::Not(testing::HasSubstr("This page has no title")));
}

}  // namespace content
