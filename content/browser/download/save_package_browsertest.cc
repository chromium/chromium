// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/save_package.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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
    std::move(callback).Run(suggested_path, save_page_type_,
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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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

class DownloadCancelObserver : public DownloadManager::Observer {
 public:
  explicit DownloadCancelObserver(base::OnceClosure canceled_closure,
                                  std::string* mime_type_out)
      : canceled_closure_(std::move(canceled_closure)),
        mime_type_out_(mime_type_out) {}
  DownloadCancelObserver(const DownloadCancelObserver&) = delete;
  DownloadCancelObserver& operator=(const DownloadCancelObserver&) = delete;

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    *mime_type_out_ = item->GetMimeType();
    DCHECK(!item_cancel_observer_);
    item_cancel_observer_ = std::make_unique<DownloadItemCancelObserver>(
        item, std::move(canceled_closure_));
  }

 private:
  class DownloadItemCancelObserver : public download::DownloadItem::Observer {
   public:
    DownloadItemCancelObserver(download::DownloadItem* item,
                               base::OnceClosure canceled_closure)
        : item_(item), canceled_closure_(std::move(canceled_closure)) {
      item_->AddObserver(this);
    }
    DownloadItemCancelObserver(const DownloadItemCancelObserver&) = delete;
    DownloadItemCancelObserver& operator=(const DownloadItemCancelObserver&) =
        delete;

    ~DownloadItemCancelObserver() override {
      if (item_)
        item_->RemoveObserver(this);
    }

   private:
    void OnDownloadUpdated(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      if (item_->GetState() == download::DownloadItem::CANCELLED)
        std::move(canceled_closure_).Run();
    }

    void OnDownloadDestroyed(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      item_->RemoveObserver(this);
      item_ = nullptr;
    }

    download::DownloadItem* item_;
    base::OnceClosure canceled_closure_;
  };

  std::unique_ptr<DownloadItemCancelObserver> item_cancel_observer_;
  base::OnceClosure canceled_closure_;
  std::string* mime_type_out_;
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
    auto* download_manager =
        static_cast<DownloadManagerImpl*>(BrowserContext::GetDownloadManager(
            shell()->web_contents()->GetBrowserContext()));
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
          new SavePackage(shell()->web_contents()));
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
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
  save_package->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemDestroyed) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_COMPLETE_HTML, true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemCanceled) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_MHTML, false);
}

// Currently, SavePageAsWebBundle feature is not implemented yet.
// WebContentsImpl::GenerateWebBundle() will call the passed callback with 0
// file size and WebBundlerError::kNotImplemented via WebBundler in the utility
// process which means it cancels all SavePageAsWebBundle requests. So this test
// checks that the request is successfully canceled.
// TODO(crbug.com/1040752): Implement WebBundler and update this test.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, SaveAsWebBundleCanceled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  auto* download_manager =
      static_cast<DownloadManagerImpl*>(BrowserContext::GetDownloadManager(
          shell()->web_contents()->GetBrowserContext()));
  auto delegate = std::make_unique<TestShellDownloadManagerDelegate>(
      SAVE_PAGE_TYPE_AS_WEB_BUNDLE);
  delegate->download_dir_ = save_dir_.GetPath();
  auto* old_delegate = download_manager->GetDelegate();
  download_manager->SetDelegate(delegate.get());

  {
    base::RunLoop run_loop;
    std::string mime_type;
    DownloadCancelObserver observer(run_loop.QuitClosure(), &mime_type);
    download_manager->AddObserver(&observer);
    scoped_refptr<SavePackage> save_package(
        new SavePackage(shell()->web_contents()));
    save_package->GetSaveInfo();
    run_loop.Run();
    download_manager->RemoveObserver(&observer);
    EXPECT_TRUE(save_package->canceled());
    EXPECT_EQ("application/webbundle", mime_type);
  }

  download_manager->SetDelegate(old_delegate);
}

}  // namespace content
