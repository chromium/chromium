// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_test_api.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace content {

namespace {
constexpr gfx::Rect kBounds = gfx::Rect(0, 0, 20, 20);
constexpr gfx::PointF kClientPt = {5, 10};
constexpr gfx::PointF kScreenPt = {17, 3};
}  // namespace

class WebContentsViewAuraTest : public RenderViewHostTestHarness {
 public:
  void OnDropComplete(RenderWidgetHostImpl* target_rwh,
                      const DropData& drop_data,
                      const gfx::PointF& client_pt,
                      const gfx::PointF& screen_pt,
                      int key_modifiers,
                      bool drop_allowed) {
    // Cache the data for verification.
    drop_complete_data_ = std::make_unique<DropCompleteData>(
        target_rwh, drop_data, client_pt, screen_pt, key_modifiers,
        drop_allowed);

    std::move(async_drop_closure_).Run();
  }

 protected:
  WebContentsViewAuraTest() = default;
  ~WebContentsViewAuraTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    root_window()->SetBounds(kBounds);
    GetNativeView()->SetBounds(kBounds);
    GetNativeView()->Show();
    root_window()->AddChild(GetNativeView());

    occluding_window_.reset(aura::test::CreateTestWindowWithDelegateAndType(
        nullptr, aura::client::WINDOW_TYPE_NORMAL, 0, kBounds, root_window(),
        false));
  }

  void TearDown() override {
    occluding_window_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  WebContentsViewAura* GetView() {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
    return static_cast<WebContentsViewAura*>(contents->GetView());
  }

  aura::Window* GetNativeView() { return web_contents()->GetNativeView(); }

  void CheckDropData(WebContentsViewAura* view) const {
    EXPECT_EQ(nullptr, view->current_drop_data_);
    ASSERT_NE(nullptr, drop_complete_data_);
    EXPECT_TRUE(drop_complete_data_->drop_allowed);
    EXPECT_EQ(view->current_rwh_for_drag_.get(),
              drop_complete_data_->target_rwh.get());
    EXPECT_EQ(kClientPt, drop_complete_data_->client_pt);
    // Screen point of event is ignored, instead cursor position used.
    EXPECT_EQ(gfx::PointF(), drop_complete_data_->screen_pt);
    EXPECT_EQ(0, drop_complete_data_->key_modifiers);
  }

  // |occluding_window_| occludes |web_contents()| when it's shown.
  std::unique_ptr<aura::Window> occluding_window_;

  // A closure indicating that async drop operation has completed.
  base::OnceClosure async_drop_closure_;

  struct DropCompleteData {
    DropCompleteData(RenderWidgetHostImpl* target_rwh,
                     const DropData& drop_data,
                     const gfx::PointF& client_pt,
                     const gfx::PointF& screen_pt,
                     int key_modifiers,
                     bool drop_allowed)
        : target_rwh(target_rwh->GetWeakPtr()),
          drop_data(drop_data),
          client_pt(client_pt),
          screen_pt(screen_pt),
          key_modifiers(key_modifiers),
          drop_allowed(drop_allowed) {}

    base::WeakPtr<RenderWidgetHostImpl> target_rwh;
    const DropData drop_data;
    const gfx::PointF client_pt;
    const gfx::PointF screen_pt;
    const int key_modifiers;
    const bool drop_allowed;
  };
  std::unique_ptr<DropCompleteData> drop_complete_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAuraTest);
};

TEST_F(WebContentsViewAuraTest, EnableDisableOverscroll) {
  WebContentsViewAura* view = GetView();
  view->SetOverscrollControllerEnabled(false);
  EXPECT_FALSE(view->gesture_nav_simple_);
  view->SetOverscrollControllerEnabled(true);
  EXPECT_TRUE(view->gesture_nav_simple_);
}

TEST_F(WebContentsViewAuraTest, ShowHideParent) {
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, OccludeView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::VISIBLE);
  occluding_window_->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::OCCLUDED);
  occluding_window_->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, DragDropFiles) {
  WebContentsViewAura* view = GetView();
  auto data = std::make_unique<ui::OSExchangeData>();

  const base::string16 string_data = base::ASCIIToUTF16("Some string data");
  data->SetString(string_data);

#if defined(OS_WIN)
  const std::vector<ui::FileInfo> test_file_infos = {
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file1")),
       base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file2")),
       base::FilePath()},
      {
          base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file3")),
          base::FilePath(),
      },
  };
#else
  const std::vector<ui::FileInfo> test_file_infos = {
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file1")), base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file2")), base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file3")), base::FilePath()},
  };
#endif
  data->SetFilenames(test_file_infos);

  ui::DropTargetEvent event(*data.get(), kClientPt, kScreenPt,
                            ui::DragDropTypes::DRAG_COPY);

  // Simulate drag enter.
  EXPECT_EQ(nullptr, view->current_drop_data_);
  view->OnDragEntered(event);
  ASSERT_NE(nullptr, view->current_drop_data_);

#if defined(USE_X11)
  // By design, OSExchangeDataProviderAuraX11::GetString returns an empty string
  // if file data is also present.
  EXPECT_TRUE(view->current_drop_data_->text.string().empty());
#else
  EXPECT_EQ(string_data, view->current_drop_data_->text.string());
#endif

  std::vector<ui::FileInfo> retrieved_file_infos =
      view->current_drop_data_->filenames;
  ASSERT_EQ(test_file_infos.size(), retrieved_file_infos.size());
  for (size_t i = 0; i < retrieved_file_infos.size(); i++) {
    EXPECT_EQ(test_file_infos[i].path, retrieved_file_infos[i].path);
    EXPECT_EQ(test_file_infos[i].display_name,
              retrieved_file_infos[i].display_name);
  }

  // Simulate drop.
  auto callback = base::BindOnce(&WebContentsViewAuraTest::OnDropComplete,
                                 base::Unretained(this));
  view->RegisterDropCallbackForTesting(std::move(callback));

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  view->OnPerformDrop(event, std::move(data));
  run_loop.Run();

  CheckDropData(view);

#if defined(USE_X11)
  // By design, OSExchangeDataProviderAuraX11::GetString returns an empty string
  // if file data is also present.
  EXPECT_TRUE(drop_complete_data_->drop_data.text.string().empty());
#else
  EXPECT_EQ(string_data, drop_complete_data_->drop_data.text.string());
#endif

  retrieved_file_infos = drop_complete_data_->drop_data.filenames;
  ASSERT_EQ(test_file_infos.size(), retrieved_file_infos.size());
  for (size_t i = 0; i < retrieved_file_infos.size(); i++) {
    EXPECT_EQ(test_file_infos[i].path, retrieved_file_infos[i].path);
    EXPECT_EQ(test_file_infos[i].display_name,
              retrieved_file_infos[i].display_name);
  }
}

#if defined(OS_WIN) || defined(USE_X11)
TEST_F(WebContentsViewAuraTest, DragDropFilesOriginateFromRenderer) {
  WebContentsViewAura* view = GetView();
  auto data = std::make_unique<ui::OSExchangeData>();

  const base::string16 string_data = base::ASCIIToUTF16("Some string data");
  data->SetString(string_data);

#if defined(OS_WIN)
  const std::vector<ui::FileInfo> test_file_infos = {
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file1")),
       base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file2")),
       base::FilePath()},
      {
          base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file3")),
          base::FilePath(),
      },
  };
#else
  const std::vector<ui::FileInfo> test_file_infos = {
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file1")), base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file2")), base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file3")), base::FilePath()},
  };
#endif
  data->SetFilenames(test_file_infos);

  // Simulate the drag originating in the renderer process, in which case
  // any file data should be filtered out (anchor drag scenario).
  data->MarkOriginatedFromRenderer();

  ui::DropTargetEvent event(*data.get(), kClientPt, kScreenPt,
                            ui::DragDropTypes::DRAG_COPY);

  // Simulate drag enter.
  EXPECT_EQ(nullptr, view->current_drop_data_);
  view->OnDragEntered(event);
  ASSERT_NE(nullptr, view->current_drop_data_);

#if defined(USE_X11)
  // By design, OSExchangeDataProviderAuraX11::GetString returns an empty string
  // if file data is also present.
  EXPECT_TRUE(view->current_drop_data_->text.string().empty());
#else
  EXPECT_EQ(string_data, view->current_drop_data_->text.string());
#endif

  ASSERT_TRUE(view->current_drop_data_->filenames.empty());

  // Simulate drop.
  auto callback = base::BindOnce(&WebContentsViewAuraTest::OnDropComplete,
                                 base::Unretained(this));
  view->RegisterDropCallbackForTesting(std::move(callback));

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  view->OnPerformDrop(event, std::move(data));
  run_loop.Run();

  CheckDropData(view);

#if defined(USE_X11)
  // By design, OSExchangeDataProviderAuraX11::GetString returns an empty string
  // if file data is also present.
  EXPECT_TRUE(drop_complete_data_->drop_data.text.string().empty());
#else
  EXPECT_EQ(string_data, drop_complete_data_->drop_data.text.string());
#endif

  ASSERT_TRUE(drop_complete_data_->drop_data.filenames.empty());
}
#endif

#if defined(OS_WIN)

// Flaky crash on ASan: http://crbug.com/1020136
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DragDropVirtualFiles DISABLED_DragDropVirtualFiles
#else
#define MAYBE_DragDropVirtualFiles DragDropVirtualFiles
#endif
TEST_F(WebContentsViewAuraTest, MAYBE_DragDropVirtualFiles) {
  WebContentsViewAura* view = GetView();
  auto data = std::make_unique<ui::OSExchangeData>();

  const base::string16 string_data = base::ASCIIToUTF16("Some string data");
  data->SetString(string_data);

  const std::vector<std::pair<base::FilePath, std::string>>
      test_filenames_and_contents = {
          {base::FilePath(FILE_PATH_LITERAL("filename.txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("another filename.txt")),
           std::string("just some data\0with\0nulls", 25)},
          {base::FilePath(FILE_PATH_LITERAL("and another filename.txt")),
           std::string("just some more data")},
      };

  data->provider().SetVirtualFileContentsForTesting(test_filenames_and_contents,
                                                    TYMED_ISTREAM);

  ui::DropTargetEvent event(*data.get(), kClientPt, kScreenPt,
                            ui::DragDropTypes::DRAG_COPY);

  // Simulate drag enter.
  EXPECT_EQ(nullptr, view->current_drop_data_);
  view->OnDragEntered(event);
  ASSERT_NE(nullptr, view->current_drop_data_);

  EXPECT_EQ(string_data, view->current_drop_data_->text.string());

  const base::FilePath path_placeholder(FILE_PATH_LITERAL("temp.tmp"));
  std::vector<ui::FileInfo> retrieved_file_infos =
      view->current_drop_data_->filenames;
  ASSERT_EQ(test_filenames_and_contents.size(), retrieved_file_infos.size());
  for (size_t i = 0; i < retrieved_file_infos.size(); i++) {
    EXPECT_EQ(test_filenames_and_contents[i].first,
              retrieved_file_infos[i].display_name);
    EXPECT_EQ(path_placeholder, retrieved_file_infos[i].path);
  }

  // Simulate drop (completes asynchronously since virtual file data is
  // present).
  auto callback = base::BindOnce(&WebContentsViewAuraTest::OnDropComplete,
                                 base::Unretained(this));
  view->RegisterDropCallbackForTesting(std::move(callback));

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  view->OnPerformDrop(event, std::move(data));
  run_loop.Run();

  CheckDropData(view);

  EXPECT_EQ(string_data, drop_complete_data_->drop_data.text.string());

  std::string read_contents;
  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  retrieved_file_infos = drop_complete_data_->drop_data.filenames;
  ASSERT_EQ(test_filenames_and_contents.size(), retrieved_file_infos.size());
  for (size_t i = 0; i < retrieved_file_infos.size(); i++) {
    EXPECT_EQ(test_filenames_and_contents[i].first,
              retrieved_file_infos[i].display_name);
    // Check if the temp files that back the virtual files are actually created
    // in the temp directory. Need to compare long file paths here because
    // GetTempDir can return a short ("8.3") path if the test is run
    // under a username that is too long.
    EXPECT_EQ(base::MakeLongFilePath(temp_dir),
              base::MakeLongFilePath(retrieved_file_infos[i].path.DirName()));
    EXPECT_EQ(test_filenames_and_contents[i].first.Extension(),
              retrieved_file_infos[i].path.Extension());
    EXPECT_TRUE(
        base::ReadFileToString(retrieved_file_infos[i].path, &read_contents));
    EXPECT_EQ(test_filenames_and_contents[i].second, read_contents);
  }
}

TEST_F(WebContentsViewAuraTest, DragDropVirtualFilesOriginateFromRenderer) {
  WebContentsViewAura* view = GetView();
  auto data = std::make_unique<ui::OSExchangeData>();

  const base::string16 string_data = base::ASCIIToUTF16("Some string data");
  data->SetString(string_data);

  const std::vector<std::pair<base::FilePath, std::string>>
      test_filenames_and_contents = {
          {base::FilePath(FILE_PATH_LITERAL("filename.txt")),
           std::string("just some data")},
          {base::FilePath(FILE_PATH_LITERAL("another filename.txt")),
           std::string("just some data\0with\0nulls", 25)},
          {base::FilePath(FILE_PATH_LITERAL("and another filename.txt")),
           std::string("just some more data")},
      };

  data->provider().SetVirtualFileContentsForTesting(test_filenames_and_contents,
                                                    TYMED_ISTREAM);

  // Simulate the drag originating in the renderer process, in which case
  // any file data should be filtered out (anchor drag scenario).
  data->MarkOriginatedFromRenderer();

  ui::DropTargetEvent event(*data.get(), kClientPt, kScreenPt,
                            ui::DragDropTypes::DRAG_COPY);

  // Simulate drag enter.
  EXPECT_EQ(nullptr, view->current_drop_data_);
  view->OnDragEntered(event);
  ASSERT_NE(nullptr, view->current_drop_data_);

  EXPECT_EQ(string_data, view->current_drop_data_->text.string());

  ASSERT_TRUE(view->current_drop_data_->filenames.empty());

  // Simulate drop (completes asynchronously since virtual file data is
  // present).
  auto callback = base::BindOnce(&WebContentsViewAuraTest::OnDropComplete,
                                 base::Unretained(this));
  view->RegisterDropCallbackForTesting(std::move(callback));

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  view->OnPerformDrop(event, std::move(data));
  run_loop.Run();

  CheckDropData(view);

  EXPECT_EQ(string_data, drop_complete_data_->drop_data.text.string());

  ASSERT_TRUE(drop_complete_data_->drop_data.filenames.empty());
}

TEST_F(WebContentsViewAuraTest, DragDropUrlData) {
  WebContentsViewAura* view = GetView();
  auto data = std::make_unique<ui::OSExchangeData>();

  const std::string url_spec = "https://www.wikipedia.org/";
  const GURL url(url_spec);
  const base::string16 url_title = base::ASCIIToUTF16("Wikipedia");
  data->SetURL(url, url_title);

  // SetUrl should also add a virtual .url (internet shortcut) file.
  std::vector<ui::FileInfo> file_infos;
  EXPECT_TRUE(data->GetVirtualFilenames(&file_infos));
  ASSERT_EQ(1ULL, file_infos.size());
  EXPECT_EQ(base::FilePath(url_title + base::ASCIIToUTF16(".url")),
            file_infos[0].display_name);

  ui::DropTargetEvent event(*data.get(), kClientPt, kScreenPt,
                            ui::DragDropTypes::DRAG_COPY);

  // Simulate drag enter.
  EXPECT_EQ(nullptr, view->current_drop_data_);
  view->OnDragEntered(event);
  ASSERT_NE(nullptr, view->current_drop_data_);

  EXPECT_EQ(url_spec, view->current_drop_data_->url);
  EXPECT_EQ(url_title, view->current_drop_data_->url_title);

  // Virtual files should not have been retrieved if url data present.
  ASSERT_TRUE(view->current_drop_data_->filenames.empty());

  // Simulate drop (completes asynchronously since virtual file data is
  // present).
  auto callback = base::BindOnce(&WebContentsViewAuraTest::OnDropComplete,
                                 base::Unretained(this));
  view->RegisterDropCallbackForTesting(std::move(callback));

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  view->OnPerformDrop(event, std::move(data));
  run_loop.Run();

  CheckDropData(view);

  EXPECT_EQ(url_spec, drop_complete_data_->drop_data.url);
  EXPECT_EQ(url_title, drop_complete_data_->drop_data.url_title);

  // Virtual files should not have been retrieved if url data present.
  ASSERT_TRUE(drop_complete_data_->drop_data.filenames.empty());
}
#endif

}  // namespace content
