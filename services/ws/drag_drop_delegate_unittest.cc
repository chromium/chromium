// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/drag_drop_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/test_change_tracker.h"
#include "services/ws/window_service_test_setup.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "url/gurl.h"

namespace ws {

class DragDropDelegateTest : public testing::Test {
 public:
  DragDropDelegateTest() = default;
  ~DragDropDelegateTest() override = default;

  // testing::Test
  void SetUp() override {
    window_ = window_tree_test_helper()->NewTopLevelWindow();

    aura::client::SetScreenPositionClient(window_->GetRootWindow(),
                                          &screen_position_client_);
  }

  void SetCanAcceptDrops(bool accepts_drops) {
    window_tree_test_helper()->window_tree()->SetCanAcceptDrops(
        window_tree_test_helper()->TransportIdForWindow(window_),
        accepts_drops);
  }

  // Simulates drag starts with |data| at given |location| in |window_|. The
  // drag is updated in UpdateDrag call before and should be completed with
  // either PerformDrop or CancelDrag.
  void StartDrag(const ui::OSExchangeData& data, const gfx::Point& location) {
    DCHECK_EQ(nullptr, drag_data_);
    drag_data_ = &data;

    delegate()->OnDragEntered(ui::DropTargetEvent(
        *drag_data_, gfx::PointF(location), gfx::PointF(location),
        ui::DragDropTypes::DRAG_MOVE));
  }

  // Simulates drag moves at given |location| in |window_|.
  void UpdateDrag(const gfx::Point& location) {
    DCHECK_NE(nullptr, drag_data_);

    delegate()->OnDragUpdated(ui::DropTargetEvent(
        *drag_data_, gfx::PointF(location), gfx::PointF(location),
        ui::DragDropTypes::DRAG_MOVE));
  }

  // Simulates drag finished with a drop at |location|.
  void PerformDrop(const gfx::Point& location) {
    DCHECK_NE(nullptr, drag_data_);

    delegate()->OnPerformDrop(ui::DropTargetEvent(
        *drag_data_, gfx::PointF(location), gfx::PointF(location),
        ui::DragDropTypes::DRAG_MOVE));
    drag_data_ = nullptr;
  }

  // Simulates a drag is canceled.
  void EndDrag() {
    DCHECK_NE(nullptr, drag_data_);

    delegate()->OnDragExited();
    drag_data_ = nullptr;
  }

  // Triggers a WindowTreeClient::OnDragDropStart and returns drag data.
  using DragDataType = aura::OSExchangeDataProviderMus::Data;
  DragDataType GetDragData(const ui::OSExchangeData& data) {
    // A point in |window()|'s coordinates.
    gfx::Point point = gfx::Rect(window()->bounds().size()).CenterPoint();

    changes()->clear();
    StartDrag(data, point);
    EndDrag();

    const Change& first_change = changes()->front();
    DCHECK_EQ(CHANGE_TYPE_DRAG_DROP_START, first_change.type);
    return mojo::FlatMapToMap(first_change.drag_data);
  }

  aura::client::DragDropDelegate* delegate() {
    return aura::client::GetDragDropDelegate(window_);
  }

  aura::Window* window() { return window_; }

  WindowTreeTestHelper* window_tree_test_helper() {
    return setup_.window_tree_test_helper();
  }

  std::vector<Change>* changes() { return setup_.changes(); }

 private:
  wm::DefaultScreenPositionClient screen_position_client_;
  WindowServiceTestSetup setup_;

  aura::Window* window_ = nullptr;
  const ui::OSExchangeData* drag_data_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DragDropDelegateTest);
};

// Tests that DragDropDelegate is created when window can accept drops and
// destroyed otherwise.
TEST_F(DragDropDelegateTest, CreateAndReset) {
  // No DragDropDelegate by default.
  EXPECT_FALSE(delegate());

  // DragDropDelegate is created when the window accepts drops.
  SetCanAcceptDrops(true);
  EXPECT_TRUE(delegate());

  // DragDropDelegate is gone when the window accepts drops.
  SetCanAcceptDrops(false);
  EXPECT_FALSE(delegate());
}

// Tests that the window tree client gets correct drag data.
TEST_F(DragDropDelegateTest, DragData) {
  SetCanAcceptDrops(true);

  // String data.
  {
    const base::string16 kTestString = base::ASCIIToUTF16("dragged string");
    ui::OSExchangeData data;
    data.SetString(kTestString);

    aura::OSExchangeDataProviderMus mus_provider(GetDragData(data));
    base::string16 dropped_string;
    EXPECT_TRUE(mus_provider.GetString(&dropped_string));
    EXPECT_EQ(kTestString, dropped_string);
  }

  // URL data.
  {
    const GURL kTestURL("http://test.com");
    const base::string16 kTestTitle = base::ASCIIToUTF16("test title");
    ui::OSExchangeData data;
    data.SetURL(kTestURL, kTestTitle);

    aura::OSExchangeDataProviderMus mus_provider(GetDragData(data));
    GURL dropped_url;
    base::string16 dropped_title;
    EXPECT_TRUE(mus_provider.GetURLAndTitle(
        ui::OSExchangeData::DO_NOT_CONVERT_FILENAMES, &dropped_url,
        &dropped_title));
    EXPECT_EQ(kTestURL, dropped_url);
    EXPECT_EQ(kTestTitle, dropped_title);
  }

#if !defined(OS_WIN)
  // File names.
  // Exclude from Windows because OSExchangeDataProviderWin works in a different
  // way and does not return the exactly the same filenames as the ones being
  // set.
  {
    // No "display_name" since OSExchangeDataProviderMus does not support it.
    const std::vector<ui::FileInfo> kTestFilenames = {
        {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file1")),
         base::FilePath()},
        {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file2")),
         base::FilePath()},
    };
    ui::OSExchangeData data;
    data.SetFilenames(kTestFilenames);

    aura::OSExchangeDataProviderMus mus_provider(GetDragData(data));
    std::vector<ui::FileInfo> dropped_filenames;
    EXPECT_TRUE(mus_provider.GetFilenames(&dropped_filenames));
    EXPECT_EQ(kTestFilenames, dropped_filenames);
  }
#endif

  // HTML
  {
    const GURL kTestURL("http://test.com");
    const base::string16 kTestHtml = base::ASCIIToUTF16(
        "<HTML>\n<BODY>\n"
        "<b>bold.</b> <i><b>This is bold italic.</b></i>\n"
        "</BODY>\n</HTML>");
    ui::OSExchangeData data;
    data.SetHtml(kTestHtml, kTestURL);

    aura::OSExchangeDataProviderMus mus_provider(GetDragData(data));
    base::string16 dropped_html;
    GURL dropped_url;
    EXPECT_TRUE(mus_provider.GetHtml(&dropped_html, &dropped_url));
    EXPECT_EQ(kTestHtml, dropped_html);
    // Not testing url because OSExchangeDataProviderMus does not support it.
  }
}

// Tests the window tree client sequence of drag enter and exit.
TEST_F(DragDropDelegateTest, EnterAndExit) {
  SetCanAcceptDrops(true);

  ui::OSExchangeData data;
  data.SetString(base::ASCIIToUTF16("dragged string"));

  // A point in |window()|'s coordinates.
  gfx::Point point = gfx::Rect(window()->bounds().size()).CenterPoint();

  changes()->clear();
  StartDrag(data, point);
  UpdateDrag(point);
  EndDrag();

  EXPECT_EQ(
      std::vector<std::string>({"DragDropStart", "DragEnter window_id=0,1",
                                "DragOver window_id=0,1",
                                "DragLeave window_id=0,1", "DragDropDone"}),
      ChangesToDescription1(*changes()));
}

// Tests the window tree client sequence of drag enter and drop.
TEST_F(DragDropDelegateTest, EnterAndDrop) {
  SetCanAcceptDrops(true);

  ui::OSExchangeData data;
  data.SetString(base::ASCIIToUTF16("dragged string"));

  // A point in |window()|'s coordinates.
  gfx::Point point = gfx::Rect(window()->bounds().size()).CenterPoint();

  changes()->clear();
  StartDrag(data, point);
  UpdateDrag(point);
  PerformDrop(point);

  EXPECT_EQ(
      std::vector<std::string>({"DragDropStart", "DragEnter window_id=0,1",
                                "DragOver window_id=0,1",
                                "CompleteDrop window_id=0,1", "DragDropDone"}),
      ChangesToDescription1(*changes()));
}

}  // namespace ws
