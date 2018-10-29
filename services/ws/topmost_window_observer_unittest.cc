// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/topmost_window_observer.h"

#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace ws {

// This class primarily tests observation of TopmostWindowObserver class.
// The actual logic of observing topmosts needs to be tested with Ash, so those
// tests are done in ash/ws/window_service_delegate_impl_unittest.cc.
class TopmostWindowObserverTest : public testing::Test {
 public:
  TopmostWindowObserverTest() = default;

  void SetUp() override {
    aura::client::SetScreenPositionClient(setup_.root(),
                                          &screen_position_client_);
  }
  void TearDown() override {
    aura::client::SetScreenPositionClient(setup_.root(), nullptr);
  }

 protected:
  aura::Window* NewWindow() {
    aura::Window* window = setup_.window_tree_test_helper()->NewWindow();
    setup_.root()->AddChild(window);
    return window;
  }
  void SetupTopmosts(aura::Window* topmost, aura::Window* real_topmost) {
    setup_.delegate()->set_topmost(topmost);
    setup_.delegate()->set_real_topmost(real_topmost);
  }
  std::unique_ptr<TopmostWindowObserver> CreateTopmostWindowObserver(
      aura::Window* window) {
    return std::make_unique<TopmostWindowObserver>(
        setup_.window_tree(), mojom::MoveLoopSource::MOUSE, window);
  }
  void UpdateTopmostWindows(TopmostWindowObserver* observer) {
    observer->UpdateTopmostWindows();
  }
  void DeleteWindow(aura::Window* window) {
    Id id = setup_.window_tree_test_helper()->TransportIdForWindow(window);
    static_cast<mojom::WindowTree*>(setup_.window_tree())->DeleteWindow(1, id);
  }

 private:
  WindowServiceTestSetup setup_;
  wm::DefaultScreenPositionClient screen_position_client_;

  DISALLOW_COPY_AND_ASSIGN(TopmostWindowObserverTest);
};

TEST_F(TopmostWindowObserverTest, BasicObserving) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w2);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, RealTopmostIsNull) {
  aura::Window* w1 = NewWindow();
  SetupTopmosts(w1, nullptr);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, TopmostIsNull) {
  aura::Window* w1 = NewWindow();
  SetupTopmosts(nullptr, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, UpdateTopmost) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));

  aura::Window* w3 = NewWindow();
  SetupTopmosts(w3, w2);
  UpdateTopmostWindows(observer.get());

  EXPECT_FALSE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
  EXPECT_TRUE(w3->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, UpdateRealTopmost) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));

  aura::Window* w3 = NewWindow();
  SetupTopmosts(w1, w3);
  UpdateTopmostWindows(observer.get());

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_FALSE(w2->HasObserver(observer.get()));
  EXPECT_TRUE(w3->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, ToSameTopmost) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w2);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));

  SetupTopmosts(w1, w1);
  UpdateTopmostWindows(observer.get());

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_FALSE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, ToSameRealTopmost) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w2);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));

  SetupTopmosts(w2, w2);
  UpdateTopmostWindows(observer.get());

  EXPECT_FALSE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, SameToDifferent) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_FALSE(w2->HasObserver(observer.get()));

  SetupTopmosts(w1, w2);
  UpdateTopmostWindows(observer.get());

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, SameToDifferent2) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_FALSE(w2->HasObserver(observer.get()));

  SetupTopmosts(w2, w1);
  UpdateTopmostWindows(observer.get());

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, SameToDifferent3) {
  aura::Window* w1 = NewWindow();
  SetupTopmosts(w1, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));

  aura::Window* w2 = NewWindow();
  aura::Window* w3 = NewWindow();

  SetupTopmosts(w2, w3);
  UpdateTopmostWindows(observer.get());

  EXPECT_FALSE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
  EXPECT_TRUE(w3->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, SameToSame) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_FALSE(w2->HasObserver(observer.get()));

  SetupTopmosts(w2, w2);
  UpdateTopmostWindows(observer.get());

  EXPECT_FALSE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, SwapObservingWindows) {
  aura::Window* w1 = NewWindow();
  aura::Window* w2 = NewWindow();
  SetupTopmosts(w1, w2);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));

  SetupTopmosts(w2, w1);
  UpdateTopmostWindows(observer.get());

  EXPECT_TRUE(w1->HasObserver(observer.get()));
  EXPECT_TRUE(w2->HasObserver(observer.get()));
}

TEST_F(TopmostWindowObserverTest, WindowDestroying) {
  aura::Window* w1 = NewWindow();
  SetupTopmosts(nullptr, w1);
  auto observer = CreateTopmostWindowObserver(w1);

  EXPECT_TRUE(w1->HasObserver(observer.get()));

  SetupTopmosts(nullptr, nullptr);
  DeleteWindow(w1);
}

}  // namespace ws
