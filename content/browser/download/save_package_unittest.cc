// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/download/save_package.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/download/save_file_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/fake_local_frame.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

#define FPL FILE_PATH_LITERAL
#define HTML_EXTENSION ".html"
#if BUILDFLAG(IS_WIN)
#define FPL_HTML_EXTENSION L".html"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define FPL_HTML_EXTENSION ".html"
#endif

namespace {

// This constant copied from save_package.cc.
#if BUILDFLAG(IS_WIN)
const uint32_t kMaxFilePathLength = MAX_PATH - 1;
const uint32_t kMaxFileNameLength = MAX_PATH - 1;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
const uint32_t kMaxFilePathLength = PATH_MAX - 1;
const uint32_t kMaxFileNameLength = NAME_MAX;
#endif

bool HasOrdinalNumber(const base::FilePath::StringType& filename) {
  base::FilePath::StringType::size_type r_paren_index =
      filename.rfind(FPL(')'));
  base::FilePath::StringType::size_type l_paren_index =
      filename.rfind(FPL('('));
  if (l_paren_index >= r_paren_index)
    return false;

  for (base::FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!base::IsAsciiDigit(filename[i]))
      return false;
  }

  return true;
}

}  // namespace

class SavePackageTest : public RenderViewHostImplTestHarness {
 public:
  bool GetGeneratedFilename(bool need_success_generate_filename,
                            const std::string& disposition,
                            const std::string& url,
                            bool need_htm_ext,
                            base::FilePath::StringType* generated_name) {
    SavePackage* save_package;
    if (need_success_generate_filename)
      save_package = save_package_success_.get();
    else
      save_package = save_package_fail_.get();
    return save_package->GenerateFileName(disposition, GURL(url), need_htm_ext,
                                          generated_name);
  }

  GURL GetUrlToBeSaved() {
    return SavePackage::GetUrlToBeSaved(contents()->GetPrimaryMainFrame());
  }

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Do the initialization in SetUp so contents() is initialized by
    // RenderViewHostImplTestHarness::SetUp.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    save_package_success_ = new SavePackage(
        contents()->GetPrimaryPage(), SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
        temp_dir_.GetPath().AppendASCII("testfile" HTML_EXTENSION),
        temp_dir_.GetPath().AppendASCII("testfile_files"));

    base::FilePath::StringType long_file_name = GetLongFileName();
    save_package_fail_ = new SavePackage(
        contents()->GetPrimaryPage(), SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
        temp_dir_.GetPath().Append(long_file_name + FPL_HTML_EXTENSION),
        temp_dir_.GetPath().Append(long_file_name + FPL("_files")));
  }

  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    // Initialize the SaveFileManager instance which we will use for the tests.
    save_file_manager_ = new SaveFileManager();
    return RenderViewHostImplTestHarness::CreateBrowserContext();
  }

  void TearDown() override {
    DeleteContents();
    base::RunLoop().RunUntilIdle();

    save_package_success_ = nullptr;
    save_package_fail_ = nullptr;

    RenderViewHostImplTestHarness::TearDown();
  }

  // Returns a path that is *almost* kMaxFilePathLength long
  base::FilePath::StringType GetLongFileName() const {
    size_t target_length =
        kMaxFilePathLength - 9 - temp_dir_.GetPath().value().length();
    return base::FilePath::StringType(target_length, FPL('a'));
  }

 private:
  // SavePackage for successfully generating file name.
  scoped_refptr<SavePackage> save_package_success_;
  // SavePackage for failed generating file name.
  scoped_refptr<SavePackage> save_package_fail_;
  base::ScopedTempDir temp_dir_;

  scoped_refptr<SaveFileManager> save_file_manager_;
};

static const struct {
  const char* disposition;
  const char* url;
  const base::FilePath::CharType* expected_name;
  bool need_htm_ext;
} kGeneratedFiles[] = {
  // We mainly focus on testing duplicated names here, since retrieving file
  // name from disposition and url has been tested in DownloadManagerTest.

  // No useful information in disposition or URL, use default.
  {"1.html", "http://www.savepage.com/",
    FPL("saved_resource") FPL_HTML_EXTENSION, true},

  // No duplicate occurs.
  {"filename=1.css", "http://www.savepage.com", FPL("1.css"), false},

  // No duplicate occurs.
  {"filename=1.js", "http://www.savepage.com", FPL("1.js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(1).css"), false},

  // No duplicate occurs.
  {"filename=1(1).js", "http://www.savepage.com", FPL("1(1).js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(2).css"), false},

  // Change number for duplicated names.
  {"filename=1(1).css", "http://www.savepage.com", FPL("1(3).css"), false},

  // No duplicate occurs.
  {"filename=1(11).css", "http://www.savepage.com", FPL("1(11).css"), false},

  // Test for case-insensitive file names.
  {"filename=readme.txt", "http://www.savepage.com",
                          FPL("readme.txt"), false},

  {"filename=readme.TXT", "http://www.savepage.com",
                          FPL("readme(1).TXT"), false},

  {"filename=READme.txt", "http://www.savepage.com",
                          FPL("readme(2).txt"), false},

  {"filename=Readme(1).txt", "http://www.savepage.com",
                          FPL("readme(3).txt"), false},
};

TEST_F(SavePackageTest, TestSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < std::size(kGeneratedFiles); ++i) {
    base::FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(true,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_TRUE(ok);
    EXPECT_EQ(kGeneratedFiles[i].expected_name, file_name);
  }
}

TEST_F(SavePackageTest, TestUnSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < std::size(kGeneratedFiles); ++i) {
    base::FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(false,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_FALSE(ok);
  }
}

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestLongSavePackageFilename DISABLED_TestLongSavePackageFilename
#else
#define MAYBE_TestLongSavePackageFilename TestLongSavePackageFilename
#endif
TEST_F(SavePackageTest, MAYBE_TestLongSavePackageFilename) {
  const std::string base_url("http://www.google.com/");
  const base::FilePath::StringType long_file_name =
      GetLongFileName() + FPL(".css");
  const std::string url =
      base_url + base::FilePath(long_file_name).AsUTF8Unsafe();

  base::FilePath::StringType filename;
  // Test that the filename is successfully shortened to fit.
  ASSERT_TRUE(GetGeneratedFilename(true, std::string(), url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file_name.length());
  EXPECT_FALSE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets an
  // an ordinal appended.
  ASSERT_TRUE(GetGeneratedFilename(true, std::string(), url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file_name.length());
  EXPECT_TRUE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets a
  // different ordinal appended.
  base::FilePath::StringType filename2;
  ASSERT_TRUE(
      GetGeneratedFilename(true, std::string(), url, false, &filename2));
  EXPECT_TRUE(filename2.length() < long_file_name.length());
  EXPECT_TRUE(HasOrdinalNumber(filename2));
  EXPECT_NE(filename, filename2);
}

// Crashing on Windows, see http://crbug.com/79365
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestLongSafePureFilename DISABLED_TestLongSafePureFilename
#else
#define MAYBE_TestLongSafePureFilename TestLongSafePureFilename
#endif
TEST_F(SavePackageTest, MAYBE_TestLongSafePureFilename) {
  const base::FilePath save_dir(FPL("test_dir"));
  const base::FilePath::StringType ext(FPL_HTML_EXTENSION);
  base::FilePath::StringType filename = GetLongFileName();

  // Test that the filename + extension doesn't exceed kMaxFileNameLength
  uint32_t max_path = SavePackage::GetMaxPathLengthForDirectory(save_dir);
  ASSERT_TRUE(SavePackage::TruncateBaseNameToFitPathConstraints(
      save_dir, ext, max_path, &filename));
  EXPECT_TRUE(filename.length() <= kMaxFileNameLength-ext.length());
}

// GetUrlToBeSaved method should return correct url to be saved.
TEST_F(SavePackageTest, TestGetUrlToBeSaved) {
  GURL url = net::URLRequestMockHTTPJob::GetMockUrl("save_page/a.htm");
  NavigateAndCommit(url);
  EXPECT_EQ(url, GetUrlToBeSaved());
}

// GetUrlToBeSaved method sould return actual url to be saved,
// instead of the displayed url used to view source of a page.
// Ex:GetUrlToBeSaved method should return http://www.google.com
// when user types view-source:http://www.google.com
TEST_F(SavePackageTest, TestGetUrlToBeSavedViewSource) {
  GURL mock_url = net::URLRequestMockHTTPJob::GetMockUrl("save_page/a.htm");
  GURL view_source_url =
      GURL(kViewSourceScheme + std::string(":") + mock_url.spec());
  GURL actual_url = net::URLRequestMockHTTPJob::GetMockUrl("save_page/a.htm");
  NavigateAndCommit(view_source_url);
  EXPECT_EQ(actual_url, GetUrlToBeSaved());
  EXPECT_EQ(view_source_url, contents()->GetLastCommittedURL());
}

class SavePackageFencedFrameTest : public SavePackageTest {
 public:
  SavePackageFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~SavePackageFencedFrameTest() override = default;

  RenderFrameHost* CreateFencedFrame(RenderFrameHost* parent) {
    RenderFrameHost* fenced_frame =
        RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// FakeLocalFrame implementation that records calls to
// GetSavableResourceLinks().
class FakeLocalFrameWithSavableResourceLinks : public FakeLocalFrame {
 public:
  explicit FakeLocalFrameWithSavableResourceLinks(RenderFrameHost* rfh) {
    auto* test_host = static_cast<TestRenderFrameHost*>(rfh);
    test_host->ResetLocalFrame();
    Init(test_host->GetRemoteAssociatedInterfaces());
  }

  bool is_called() const { return is_called_; }

  // FakeLocalFrame:
  void GetSavableResourceLinks(
      GetSavableResourceLinksCallback callback) override {
    is_called_ = true;
    std::move(callback).Run(nullptr);
  }

 private:
  bool is_called_ = false;
};

// Tests that SavePackage does not create an unnecessary task that gets the
// resources links from fenced frames.
// If fenced frames become savable, this test will need to be updated.
// See https://crbug.com/1321102
TEST_F(SavePackageFencedFrameTest,
       DontRequestSavableResourcesFromFencedFrames) {
  NavigateAndCommit(GURL("https://www.example.com"));

  // Create a fenced frame.
  RenderFrameHostTester::For(contents()->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(contents()->GetPrimaryMainFrame());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SavePackage> save_package(new SavePackage(
      contents()->GetPrimaryPage(), SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
      temp_dir.GetPath().AppendASCII("testfile" HTML_EXTENSION),
      temp_dir.GetPath().AppendASCII("testfile_files")));

  FakeLocalFrameWithSavableResourceLinks local_frame_for_primary(
      contents()->GetPrimaryMainFrame());
  local_frame_for_primary.Init(
      contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces());

  FakeLocalFrameWithSavableResourceLinks local_frame_for_fenced_frame(
      fenced_frame_rfh);
  local_frame_for_fenced_frame.Init(
      fenced_frame_rfh->GetRemoteAssociatedInterfaces());

  EXPECT_TRUE(save_package->Init(base::DoNothing()));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(local_frame_for_primary.is_called());
  EXPECT_FALSE(local_frame_for_fenced_frame.is_called());
}

}  // namespace content
