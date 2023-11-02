// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/touch_passthrough_manager.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"

using ::testing::ContainerEq;

namespace content {

// Subclass of TouchPassthroughManager enabling us to unit-test it.
class TestTouchPassthroughManager : public TouchPassthroughManager {
 public:
  TestTouchPassthroughManager() : TouchPassthroughManager(nullptr) {
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = {2, 3};
    root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

    ui::AXNodeData target1;
    target1.id = 2;
    target1.AddBoolAttribute(ax::mojom::BoolAttribute::kTouchPassthrough, true);
    target1.relative_bounds.bounds = gfx::RectF(100, 100, 400, 100);

    ui::AXNodeData target2;
    target2.id = 3;
    target2.AddBoolAttribute(ax::mojom::BoolAttribute::kTouchPassthrough, true);
    target2.relative_bounds.bounds = gfx::RectF(100, 200, 400, 100);

    browser_accessibility_manager_.reset(BrowserAccessibilityManager::Create(
        MakeAXTreeUpdateForTesting(root, target1, target2), nullptr));
  }

  ~TestTouchPassthroughManager() override = default;

  const std::vector<std::string> event_log() { return event_log_; }

 private:
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;
  std::vector<std::string> event_log_;
  base::TimeTicks previous_time_;
  bool button_down_ = false;

  // This is needed to prevent a DCHECK failure when OnAccessibilityApiUsage
  // is called in BrowserAccessibility::GetRole.
  content::BrowserTaskEnvironment task_environment_;

  void SendHitTest(
      const gfx::Point& point_in_frame_pixels,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> callback) override {
    BrowserAccessibility* result =
        browser_accessibility_manager_->GetBrowserAccessibilityRoot()
            ->ApproximateHitTest(point_in_frame_pixels);
    ui::AXNodeID hit_node_id = result ? result->GetId() : ui::kInvalidAXNodeID;
    std::move(callback).Run(browser_accessibility_manager_.get(), hit_node_id);
  }

  void CancelTouchesAndDestroyTouchDriver() override {}

  void SimulatePress(const gfx::Point& point,
                     const base::TimeTicks& time) override {
    EXPECT_FALSE(button_down_);
    EXPECT_GE(time, previous_time_);
    previous_time_ = time;
    button_down_ = true;
    event_log_.push_back(
        base::StringPrintf("Press %d, %d", point.x(), point.y()));
  }

  void SimulateMove(const gfx::Point& point,
                    const base::TimeTicks& time) override {
    EXPECT_TRUE(button_down_);
    EXPECT_GE(time, previous_time_);
    previous_time_ = time;
    event_log_.push_back(
        base::StringPrintf("Move %d, %d", point.x(), point.y()));
  }

  void SimulateRelease(const base::TimeTicks& time) override {
    EXPECT_TRUE(button_down_);
    EXPECT_GE(time, previous_time_);
    previous_time_ = time;
    button_down_ = false;
    event_log_.push_back("Release");
  }
};

class TouchPassthroughManagerTest : public testing::Test {
 public:
  TouchPassthroughManagerTest() = default;

  TouchPassthroughManagerTest(const TouchPassthroughManagerTest&) = delete;
  TouchPassthroughManagerTest& operator=(const TouchPassthroughManagerTest&) =
      delete;

  ~TouchPassthroughManagerTest() override = default;

 protected:
 private:
  void SetUp() override {}
};

TEST_F(TouchPassthroughManagerTest, TapOutsidePassthroughRegion) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(5, 5));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(), ContainerEq(std::vector<std::string>()));
}

TEST_F(TouchPassthroughManagerTest, TapInPassthroughRegion) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(105, 105));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(), ContainerEq(std::vector<std::string>{
                                       "Press 105, 105", "Release"}));
}

TEST_F(TouchPassthroughManagerTest, DragWithinPassthroughRegion) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(105, 105));
  manager.OnTouchMove(gfx::Point(495, 195));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(),
              ContainerEq(std::vector<std::string>{
                  "Press 105, 105", "Move 495, 195", "Release"}));
}

TEST_F(TouchPassthroughManagerTest, DragOutOfPassthroughRegion) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(105, 105));
  manager.OnTouchMove(gfx::Point(105, 95));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(),
              ContainerEq(std::vector<std::string>{"Press 105, 105",
                                                   "Move 105, 95", "Release"}));
}

TEST_F(TouchPassthroughManagerTest, DragIntoPassthroughRegion) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(80, 80));
  manager.OnTouchMove(gfx::Point(90, 90));
  manager.OnTouchMove(gfx::Point(110, 110));
  manager.OnTouchMove(gfx::Point(120, 120));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(), ContainerEq(std::vector<std::string>{}));
}

TEST_F(TouchPassthroughManagerTest, DragBetweenPassthroughRegions) {
  TestTouchPassthroughManager manager;
  manager.OnTouchStart(gfx::Point(110, 180));
  manager.OnTouchMove(gfx::Point(110, 190));
  manager.OnTouchMove(gfx::Point(110, 210));
  manager.OnTouchMove(gfx::Point(110, 220));
  manager.OnTouchEnd();
  EXPECT_THAT(manager.event_log(),
              ContainerEq(std::vector<std::string>{
                  "Press 110, 180", "Move 110, 190", "Move 110, 210",
                  "Move 110, 220", "Release"}));
}

}  // namespace content
