// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

class MemoryPurgeManagerTest : public testing::Test {
 public:
  MemoryPurgeManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        memory_purge_manager_(task_environment_.GetMainThreadTaskRunner()) {}
  MemoryPurgeManagerTest(const MemoryPurgeManagerTest&) = delete;
  MemoryPurgeManagerTest& operator=(const MemoryPurgeManagerTest&) = delete;

  void SetUp() override {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE,
        base::BindRepeating(&MemoryPurgeManagerTest::OnMemoryPressure,
                            base::Unretained(this)));
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  void TearDown() override {
    memory_pressure_listener_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
    memory_purge_manager_.SetPurgeDisabledForTesting(false);
  }

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  unsigned MemoryPressureCount() const { return memory_pressure_count_; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  MemoryPurgeManager memory_purge_manager_;

  unsigned memory_pressure_count_ = 0;

 private:
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel) {
    memory_pressure_count_++;
  }
};

// Verify that OnPageFrozen() triggers a memory pressure notification in a
// backgrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInBackgroundedRenderer) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();
  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1U, MemoryPressureCount());
}

// Verify that OnPageFrozen() does not trigger a memory pressure notification in
// a foregrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInForegroundedRenderer) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();
  memory_purge_manager_.SetRendererBackgrounded(false);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::Minutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PageResumedUndoMemoryPressureSuppression) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());

  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
}

TEST_F(MemoryPurgeManagerTest, PageFrozenPurgeMemoryAllPagesFrozenDisabled) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated();
  memory_purge_manager_.OnPageCreated();
  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(2U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(3U, MemoryPressureCount());
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageCreated();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
  memory_purge_manager_.OnPageDestroyed(/* frozen=*/true);
  memory_purge_manager_.OnPageDestroyed(/* frozen=*/true);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeAfterFreeze) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();

  // The memory pressure notification happens soon, in a differnt task.
  EXPECT_EQ(0U, MemoryPressureCount());
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/true);
}

TEST_F(MemoryPurgeManagerTest, CancelMemoryPurgeAfterFreeze) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  EXPECT_EQ(0U, MemoryPressureCount());

  // If the page is resumed before the memory purge timer expires, the purge
  // should be cancelled.
  memory_purge_manager_.OnPageResumed();
  FastForwardBy(base::Seconds(0));
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelayNewActivePageCreated) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  EXPECT_EQ(0U, MemoryPressureCount());

  // Some page is sill frozen, keep going.
  memory_purge_manager_.OnPageCreated();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/true);
  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
}

TEST_F(MemoryPurgeManagerTest, PurgeRendererMemoryWhenBackgroundedEnabled) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded);
  // No page, no memory pressure.
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PurgeRendererMemoryWhenBackgroundedDisabled) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(base::TimeDelta::Max());
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest,
       PurgeRendererMemoryWhenBackgroundedEnabledForegroundedBeforePurge) {
  if (!MemoryPurgeManager::kPurgeEnabled) {
    GTEST_SKIP();
  }

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(base::Seconds(30));
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.SetRendererBackgrounded(false);
  FastForwardBy(base::TimeDelta::Max());
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PageFrozenAndResumedWhileBackgrounded) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay / 2);
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.OnPageResumed();
  FastForwardBy(MemoryPurgeManager::kFreezePurgeDelay);
  // Since the renderer is still backgrounded, the memory purge should happen
  // even though there are no frozen pages.
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(/* frozen=*/false);
}

TEST_F(MemoryPurgeManagerTest, NoMemoryPurgeIfNoPage) {
  memory_purge_manager_.SetPurgeDisabledForTesting(true);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageCreated();

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  memory_purge_manager_.OnPageDestroyed(/* frozen=*/true);

  FastForwardBy(base::Minutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
}

}  // namespace

}  // namespace blink
