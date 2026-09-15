// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/directory.h>
#include <lib/fdio/namespace.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/remote_dir.h>
#include <lib/vfs/cpp/vmo_file.h>

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_test_suite_base.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/content_directory_loader_factory.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace {

// Adds an in-memory file containing |data| to |dir| at the location |path|.
void AddFileToPseudoDir(std::string_view data,
                        const base::FilePath& path,
                        vfs::PseudoDir* dir) {
  zx::vmo contents_vmo;
  zx_status_t status = zx::vmo::create(data.size(), 0, &contents_vmo);
  ASSERT_EQ(status, ZX_OK);
  status = contents_vmo.write(data.data(), 0, data.size());
  ASSERT_EQ(status, ZX_OK);

  auto vmo_file = std::make_unique<vfs::VmoFile>(
      std::move(contents_vmo), data.size(), vfs::VmoFile::WriteMode::kReadOnly,
      vfs::VmoFile::DefaultSharingMode::kCloneCow);
  status = dir->AddEntry(path.value(), std::move(vmo_file));
  ASSERT_EQ(status, ZX_OK);
}

class TestContentDirectory {
 public:
  TestContentDirectory() {
    directory_serving_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));

    fidl::InterfaceHandle<fuchsia::io::Directory> handle;
    directory_serving_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](vfs::PseudoDir* root_dir,
                          fidl::ServerEnd<fuchsia_io::Directory> server_end) {
                         zx_status_t status =
                             root_dir->Serve(fuchsia_io::wire::kPermReadable,
                                             std::move(server_end));
                         ZX_CHECK(status == ZX_OK, status);
                       },
                       base::Unretained(&root_content_directory_),
                       fidl::ServerEnd<fuchsia_io::Directory>(
                           handle.NewRequest().TakeChannel())));

    zx_status_t status = fdio_ns_get_installed(&namespace_);
    ZX_CHECK(status == ZX_OK, status);
    status = fdio_ns_bind(
        namespace_, ContentDirectoryLoaderFactory::kContentDirectoriesPath,
        handle.TakeChannel().release());
    ZX_CHECK(status == ZX_OK, status);
  }

  ~TestContentDirectory() {
    if (namespace_) {
      zx_status_t status = fdio_ns_unbind(
          namespace_, ContentDirectoryLoaderFactory::kContentDirectoriesPath);
      ZX_CHECK(status == ZX_OK, status);
    }

    directory_serving_thread_.Stop();
  }

  TestContentDirectory(const TestContentDirectory&) = delete;
  TestContentDirectory& operator=(const TestContentDirectory&) = delete;

  vfs::PseudoDir* root_dir() { return &root_content_directory_; }

 private:
  vfs::PseudoDir root_content_directory_;
  base::Thread directory_serving_thread_{"content-directory-serving"};
  fdio_ns_t* namespace_ = nullptr;
};

// Sets the specified directory as a ContentDirectory under the path |name|.
class ScopedBindContentDirectory {
 public:
  ScopedBindContentDirectory(TestContentDirectory* test_content_directory,
                             std::string_view name,
                             std::unique_ptr<vfs::PseudoDir> pseudo_dir)
      : test_content_directory_(test_content_directory), name_(name) {
    zx_status_t status = test_content_directory_->root_dir()->AddEntry(
        name_, std::move(pseudo_dir));
    ZX_CHECK(status == ZX_OK, status);
  }
  ScopedBindContentDirectory(
      TestContentDirectory* test_content_directory,
      std::string_view name,
      fidl::InterfaceHandle<fuchsia::io::Directory> directory_channel)
      : test_content_directory_(test_content_directory), name_(name) {
    zx_status_t status = test_content_directory_->root_dir()->AddEntry(
        name_,
        std::make_unique<vfs::RemoteDir>(directory_channel.TakeChannel()));
    ZX_CHECK(status == ZX_OK, status);
  }
  ~ScopedBindContentDirectory() {
    zx_status_t status =
        test_content_directory_->root_dir()->RemoveEntry(name_);
    ZX_CHECK(status == ZX_OK, status);
  }

 private:
  TestContentDirectory* const test_content_directory_;
  const std::string name_;
};

class ContentDirectoryTest : public WebEngineBrowserTest {
 public:
  ContentDirectoryTest() = default;
  ~ContentDirectoryTest() override = default;

  ContentDirectoryTest(const ContentDirectoryTest&) = delete;
  ContentDirectoryTest& operator=(const ContentDirectoryTest&) = delete;

  void SetUp() override {
    // Set this flag early so that the fuchsia-dir:// scheme will be
    // registered at browser startup.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "enable-content-directories");

    // Scheme initialization for the WebEngineContentClient depends on the above
    // command line modification, which won't have been present when the schemes
    // were initially registered.
    content::ContentTestSuiteBase::ReRegisterContentSchemes();

    WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    test_content_directory_ = std::make_unique<TestContentDirectory>();

    base::FilePath pkg_path;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path));

    testdata_content_directory_ = std::make_unique<ScopedBindContentDirectory>(
        test_content_directory_.get(), "testdata",
        base::OpenDirectoryHandle(
            pkg_path.AppendASCII("fuchsia_web/webengine/test/data")));
    alternate_content_directory_ = std::make_unique<ScopedBindContentDirectory>(
        test_content_directory_.get(), "alternate",
        base::OpenDirectoryHandle(
            pkg_path.AppendASCII("fuchsia_web/webengine/test/data")));

    WebEngineBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    testdata_content_directory_.reset();
    alternate_content_directory_.reset();
    test_content_directory_.reset();

    WebEngineBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TestContentDirectory> test_content_directory_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;

  std::unique_ptr<ScopedBindContentDirectory> testdata_content_directory_;
  std::unique_ptr<ScopedBindContentDirectory> alternate_content_directory_;
};

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, Navigate) {
  const GURL kUrl("fuchsia-dir://testdata/title1.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlEquals(kUrl);
}

// Navigate to a resource stored under a secondary provider.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, NavigateAlternate) {
  const GURL kUrl("fuchsia-dir://alternate/title1.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlEquals(kUrl);
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ScriptSubresource) {
  const GURL kUrl("fuchsia-dir://testdata/include_script.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl,
                                                        "title set by script");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ImgSubresource) {
  const GURL kUrl("fuchsia-dir://testdata/include_image.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "image fetched");
}

// Reads content sourced from VFS PseudoDirs and VmoFiles.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, FromVfsPseudoDir) {
  base::ScopedAllowBlockingForTesting allow_block;

  std::string contents;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path);
  ASSERT_TRUE(base::ReadFileToString(
      pkg_path.AppendASCII("fuchsia_web/webengine/test/data/title1.html"),
      &contents));

  auto pseudo_dir = std::make_unique<vfs::PseudoDir>();
  AddFileToPseudoDir(contents, base::FilePath("title1.html"), pseudo_dir.get());
  ScopedBindContentDirectory test_directory(
      test_content_directory_.get(), "pseudo-dir", std::move(pseudo_dir));

  // Access the VmoFile under the PseudoDir.
  const GURL kUrl("fuchsia-dir://pseudo-dir/title1.html");
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "title 1");
}

// Verify that resource providers are origin-isolated.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ScriptSrcCrossOriginBlocked) {
  const GURL kUrl("fuchsia-dir://testdata/cross_origin_include_script.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  // If the cross-origin script succeeded, then we should see "title set by
  // script". If "not clobbered" remains set, then we know that CROS enforcement
  // is working.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl,
                                                        "same origin ftw");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, CrossOriginImgBlocked) {
  const GURL kUrl("fuchsia-dir://testdata/cross_origin_include_image.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));

  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "image rejected");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, MetadataFileParsed) {
  const GURL kUrl("fuchsia-dir://testdata/mime_override.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kUrl, "content-type: text/bleep; charset=US-ASCII");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, BadMetadataFile) {
  const GURL kUrl("fuchsia-dir://testdata/mime_override_invalid.html");

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kUrl, "content-type: text/html");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, BigFilesAreSniffable) {
  base::ScopedAllowBlockingForTesting allow_block;

  std::string contents;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path);
  ASSERT_TRUE(base::ReadFileToString(
      pkg_path.AppendASCII(
          "fuchsia_web/webengine/test/data/mime_override.html"),
      &contents));

  auto pseudo_dir = std::make_unique<vfs::PseudoDir>();
  AddFileToPseudoDir(contents, base::FilePath("test.html"), pseudo_dir.get());

  // Produce an HTML file that's a megabyte in size by appending a lot of
  // zeroes to the end of an existing HTML file.
  contents.resize(1000000, ' ');
  AddFileToPseudoDir(contents, base::FilePath("blob.bin"), pseudo_dir.get());
  ScopedBindContentDirectory test_directory(
      test_content_directory_.get(), "pseudo-dir", std::move(pseudo_dir));

  // Access the VmoFile under the PseudoDir.
  const GURL kUrl("fuchsia-dir://pseudo-dir/test.html");
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kUrl, "content-type: text/html");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, InvalidContentDirectoryName) {
  for (const char* url_spec : {
           "fuchsia-dir://./title1.html",
           "fuchsia-dir://./testdata/title1.html",
           "fuchsia-dir://../title1.html",
           "fuchsia-dir://../content-directories/testdata/title1.html",
           "fuchsia-dir://test*data/title1.html",
           "fuchsia-dir://test@data/title1.html",
       }) {
    auto frame =
        FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         url_spec));
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }
}

}  // namespace
