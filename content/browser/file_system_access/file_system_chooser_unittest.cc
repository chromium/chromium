// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_chooser.h"

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

class FileSystemChooserTest : public RenderViewHostImplTestHarness {
 public:
  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  std::vector<PathInfo> SyncShowDialog(
      WebContents* web_contents,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           std::vector<PathInfo>>
        future;
    FileSystemChooser::CreateAndShow(
        web_contents,
        FileSystemChooser::Options(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                   blink::mojom::AcceptsTypesInfo::New(
                                       std::move(accepts), include_accepts_all),
                                   std::u16string(), base::FilePath(),
                                   base::FilePath()),
        future.GetCallback(), base::ScopedClosureRunner());
    return std::get<1>(future.Take());
  }

 protected:
  std::unique_ptr<content::WebContents> CreateTestWebContents(
      content::BrowserContext* browser_context) {
    auto site_instance = content::SiteInstance::Create(browser_context);
    return content::WebContentsTester::CreateTestWebContents(
        browser_context, std::move(site_instance));
  }

  // Must persist throughout TearDown().
  SelectFileDialogParams dialog_params_;
};

TEST_F(FileSystemChooserTest, EmptyAccepts) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  SyncShowDialog(/*web_contents=*/nullptr, {}, /*include_accepts_all=*/true);

  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_TRUE(dialog_params_.file_types->include_all_files);
  EXPECT_EQ(0u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(0u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(0, dialog_params_.file_type_index);
}

TEST_F(FileSystemChooserTest, EmptyAcceptsIgnoresIncludeAcceptsAll) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  SyncShowDialog(/*web_contents=*/nullptr, {}, /*include_accepts_all=*/false);

  // Should still include_all_files, even though include_accepts_all was false.
  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_TRUE(dialog_params_.file_types->include_all_files);
  EXPECT_EQ(0u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(0u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(0, dialog_params_.file_type_index);
}

TEST_F(FileSystemChooserTest, AcceptsMimeTypes) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"", std::vector<std::string>({"tExt/Plain"}),
      std::vector<std::string>({})));
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"Images", std::vector<std::string>({"image/*"}),
      std::vector<std::string>({})));
  SyncShowDialog(/*web_contents=*/nullptr, std::move(accepts),
                 /*include_accepts_all=*/true);

  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_TRUE(dialog_params_.file_types->include_all_files);
  ASSERT_EQ(2u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(1, dialog_params_.file_type_index);

  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[0],
                             FILE_PATH_LITERAL("text")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[0],
                             FILE_PATH_LITERAL("txt")));

  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[1],
                             FILE_PATH_LITERAL("gif")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpg")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpeg")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[1],
                             FILE_PATH_LITERAL("png")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[1],
                             FILE_PATH_LITERAL("tiff")));

  ASSERT_EQ(2u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(u"", dialog_params_.file_types->extension_description_overrides[0]);
  EXPECT_EQ(u"Images",
            dialog_params_.file_types->extension_description_overrides[1]);
}

TEST_F(FileSystemChooserTest, AcceptsExtensions) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"", std::vector<std::string>({}),
      std::vector<std::string>({"text", "js", "text"})));
  SyncShowDialog(/*web_contents=*/nullptr, std::move(accepts),
                 /*include_accepts_all=*/true);

  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_TRUE(dialog_params_.file_types->include_all_files);
  ASSERT_EQ(1u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(1, dialog_params_.file_type_index);

  ASSERT_EQ(2u, dialog_params_.file_types->extensions[0].size());
  EXPECT_EQ(dialog_params_.file_types->extensions[0][0],
            FILE_PATH_LITERAL("text"));
  EXPECT_EQ(dialog_params_.file_types->extensions[0][1],
            FILE_PATH_LITERAL("js"));

  ASSERT_EQ(1u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(u"", dialog_params_.file_types->extension_description_overrides[0]);
}

TEST_F(FileSystemChooserTest, AcceptsExtensionsAndMimeTypes) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"", std::vector<std::string>({"image/*"}),
      std::vector<std::string>({"text", "jpg"})));
  SyncShowDialog(/*web_contents=*/nullptr, std::move(accepts),
                 /*include_accepts_all=*/false);

  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_FALSE(dialog_params_.file_types->include_all_files);
  ASSERT_EQ(1u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(1, dialog_params_.file_type_index);

  ASSERT_GE(dialog_params_.file_types->extensions[0].size(), 4u);
  EXPECT_EQ(dialog_params_.file_types->extensions[0][0],
            FILE_PATH_LITERAL("text"));
  EXPECT_EQ(dialog_params_.file_types->extensions[0][1],
            FILE_PATH_LITERAL("jpg"));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[0],
                             FILE_PATH_LITERAL("gif")));
  EXPECT_TRUE(base::Contains(dialog_params_.file_types->extensions[0],
                             FILE_PATH_LITERAL("jpeg")));
  EXPECT_EQ(1, base::ranges::count(dialog_params_.file_types->extensions[0],
                                   FILE_PATH_LITERAL("jpg")));

  ASSERT_EQ(1u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(u"", dialog_params_.file_types->extension_description_overrides[0]);
}

TEST_F(FileSystemChooserTest, IgnoreShellIntegratedExtensions) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"", std::vector<std::string>({}),
      std::vector<std::string>(
          {"lnk", "foo.lnk", "foo.bar.local", "text", "local", "scf", "url"})));
  SyncShowDialog(/*web_contents=*/nullptr, std::move(accepts),
                 /*include_accepts_all=*/false);

  ASSERT_TRUE(dialog_params_.file_types);
  EXPECT_FALSE(dialog_params_.file_types->include_all_files);
  ASSERT_EQ(1u, dialog_params_.file_types->extensions.size());
  EXPECT_EQ(1, dialog_params_.file_type_index);

  ASSERT_EQ(1u, dialog_params_.file_types->extensions[0].size());
  EXPECT_EQ(dialog_params_.file_types->extensions[0][0],
            FILE_PATH_LITERAL("text"));

  ASSERT_EQ(1u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(u"", dialog_params_.file_types->extension_description_overrides[0]);
}

TEST_F(FileSystemChooserTest, LocalPath) {
  const base::FilePath local_path(FILE_PATH_LITERAL("/foo/bar"));
  ui::SelectedFileInfo selected_file(local_path, local_path);

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<ui::SelectedFileInfo>{selected_file}));
  auto results = SyncShowDialog(/*web_contents=*/nullptr, {},
                                /*include_accepts_all=*/true);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].type, PathType::kLocal);
  EXPECT_EQ(results[0].path, local_path);
}

TEST_F(FileSystemChooserTest, ExternalPath) {
  const base::FilePath local_path(FILE_PATH_LITERAL("/foo/bar"));
  const base::FilePath virtual_path(
      FILE_PATH_LITERAL("/some/virtual/path/filename"));
  ui::SelectedFileInfo selected_file(local_path, local_path);
  selected_file.virtual_path = virtual_path;

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<ui::SelectedFileInfo>{selected_file}));
  auto results = SyncShowDialog(/*web_contents=*/nullptr, {},
                                /*include_accepts_all=*/true);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].type, PathType::kExternal);
  EXPECT_EQ(results[0].path, virtual_path);
}

TEST_F(FileSystemChooserTest, DescriptionSanitization) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"Description        with \t      a  \r   lot   of  \n "
      u"                                 spaces",
      std::vector<std::string>({}), std::vector<std::string>({"txt"})));
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"Description that is very long and should be "
      u"truncated to 64 code points if it works",
      std::vector<std::string>({}), std::vector<std::string>({"js"})));
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"Unbalanced RTL \u202e section", std::vector<std::string>({}),
      std::vector<std::string>({"js"})));
  accepts.emplace_back(blink::mojom::ChooseFileSystemEntryAcceptsOption::New(
      u"Unbalanced RTL \u202e section in a otherwise "
      u"very long description that will be truncated",
      std::vector<std::string>({}), std::vector<std::string>({"js"})));
  SyncShowDialog(/*web_contents=*/nullptr, std::move(accepts),
                 /*include_accepts_all=*/false);

  ASSERT_TRUE(dialog_params_.file_types);
  ASSERT_EQ(4u,
            dialog_params_.file_types->extension_description_overrides.size());
  EXPECT_EQ(u"Description with a lot of spaces",
            dialog_params_.file_types->extension_description_overrides[0]);
  EXPECT_EQ(u"Description that is very long and should be truncated to 64 cod…",
            dialog_params_.file_types->extension_description_overrides[1]);
  EXPECT_EQ(u"Unbalanced RTL \u202e section\u202c",
            dialog_params_.file_types->extension_description_overrides[2]);
  EXPECT_EQ(
      u"Unbalanced RTL \u202e section in a "
      u"otherwise very long description t…\u202c",
      dialog_params_.file_types->extension_description_overrides[3]);
}

TEST_F(FileSystemChooserTest, DialogCaller) {
  std::unique_ptr<WebContents> web_contents =
      CreateTestWebContents(GetBrowserContext());
  const GURL gurl("https://www.example.com");
  content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(gurl);

  ui::SelectFileDialog::SetFactory(
      std::make_unique<CancellingSelectFileDialogFactory>(&dialog_params_));
  SyncShowDialog(web_contents.get(), {}, /*include_accepts_all=*/true);

  ASSERT_TRUE(dialog_params_.caller.has_value());
  EXPECT_EQ(dialog_params_.caller.value(), gurl);
}

}  // namespace content
