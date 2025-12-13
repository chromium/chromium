// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/delegating_desktop_display_info_monitor.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;

MATCHER_P(DisplayCountIs, count, "") {
  return arg.NumDisplays() == count;
}

class FakeDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  void Start() override { started = true; }

  bool IsStarted() const override { return started; }

  const DesktopDisplayInfo* GetLatestDisplayInfo() const override {
    return info ? &info.value() : nullptr;
  }

  void AddCallback(base::RepeatingClosure callback) override {
    callbacks.AddUnsafe(std::move(callback));
  }

  void NotifyChange() { callbacks.Notify(); }

  void AddFakeDisplay() {
    if (!info) {
      info.emplace();
    }
    info->AddDisplay(DisplayGeometry(info->displays().size(), 0, 0, 1920, 1080,
                                     96, 32, true, ""));
  }

  base::WeakPtr<FakeDesktopDisplayInfoMonitor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool started = false;
  std::optional<DesktopDisplayInfo> info;
  base::RepeatingClosureList callbacks;

 private:
  base::WeakPtrFactory<FakeDesktopDisplayInfoMonitor> weak_ptr_factory_{this};
};

}  // namespace

class DelegatingDesktopDisplayInfoMonitorTest : public testing::Test {
 public:
  MOCK_METHOD(void, OnDisplayInfoChanged, ());

 protected:
  void AddCallback();

  base::test::TaskEnvironment task_environment_;
  FakeDesktopDisplayInfoMonitor underlying_monitor_;
  DelegatingDesktopDisplayInfoMonitor monitor_{
      underlying_monitor_.GetWeakPtr()};
};

void DelegatingDesktopDisplayInfoMonitorTest::AddCallback() {
  monitor_.AddCallback(base::BindRepeating(
      &DelegatingDesktopDisplayInfoMonitorTest::OnDisplayInfoChanged,
      base::Unretained(this)));
}

TEST_F(DelegatingDesktopDisplayInfoMonitorTest, SeparateStartedState) {
  underlying_monitor_.started = true;
  underlying_monitor_.AddFakeDisplay();

  ASSERT_FALSE(monitor_.IsStarted());
  ASSERT_EQ(monitor_.GetLatestDisplayInfo(), nullptr);
}

TEST_F(DelegatingDesktopDisplayInfoMonitorTest,
       StartWhenUnderlyingIsNotStarted) {
  ASSERT_FALSE(underlying_monitor_.IsStarted());
  EXPECT_CALL(*this, OnDisplayInfoChanged()).Times(0);

  monitor_.Start();

  ASSERT_TRUE(monitor_.IsStarted());
  ASSERT_EQ(monitor_.GetLatestDisplayInfo(), nullptr);
  ASSERT_TRUE(underlying_monitor_.IsStarted());
  ASSERT_FALSE(underlying_monitor_.callbacks.empty());
}

TEST_F(DelegatingDesktopDisplayInfoMonitorTest,
       StartWhenUnderlyingIsStartedWithNoInfo_CallbackNotNotified) {
  underlying_monitor_.started = true;
  AddCallback();
  ASSERT_TRUE(underlying_monitor_.IsStarted());
  EXPECT_CALL(*this, OnDisplayInfoChanged()).Times(0);

  monitor_.Start();

  ASSERT_TRUE(monitor_.IsStarted());
  ASSERT_EQ(monitor_.GetLatestDisplayInfo(), nullptr);
  ASSERT_TRUE(underlying_monitor_.IsStarted());
  ASSERT_FALSE(underlying_monitor_.callbacks.empty());
}

TEST_F(DelegatingDesktopDisplayInfoMonitorTest,
       StartWhenUnderlyingIsStartedWithInfo_CallbackNotifiedOnNextTask) {
  underlying_monitor_.started = true;
  underlying_monitor_.AddFakeDisplay();
  AddCallback();
  ASSERT_TRUE(underlying_monitor_.IsStarted());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnDisplayInfoChanged())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  monitor_.Start();
  run_loop.Run();

  ASSERT_TRUE(monitor_.IsStarted());
  ASSERT_NE(monitor_.GetLatestDisplayInfo(), nullptr);
  ASSERT_TRUE(underlying_monitor_.IsStarted());
  ASSERT_FALSE(underlying_monitor_.callbacks.empty());
}

TEST_F(DelegatingDesktopDisplayInfoMonitorTest, UnderlyingUpdate) {
  underlying_monitor_.started = true;
  underlying_monitor_.AddFakeDisplay();
  AddCallback();
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnDisplayInfoChanged())
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  monitor_.Start();
  run_loop.Run();
  ASSERT_EQ(monitor_.GetLatestDisplayInfo()->NumDisplays(), 1);

  EXPECT_CALL(*this, OnDisplayInfoChanged()).Times(1);

  underlying_monitor_.AddFakeDisplay();
  underlying_monitor_.NotifyChange();
  ASSERT_EQ(monitor_.GetLatestDisplayInfo()->NumDisplays(), 2);
}

}  // namespace remoting
