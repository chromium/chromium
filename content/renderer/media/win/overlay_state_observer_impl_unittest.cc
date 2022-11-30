// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/overlay_state_observer_impl.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

class MockOverlayStateService : public OverlayStateServiceProvider {
 public:
  MockOverlayStateService() = default;

  bool RegisterObserver(mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
                            overlay_state_observer,
                        const gpu::Mailbox& mailbox) override {
    if (overlay_state_observer_.is_bound()) {
      overlay_state_observer_.reset();
    }

    overlay_state_observer_.Bind(std::move(overlay_state_observer));
    mailbox_ = std::move(mailbox);
    return true;
  }

  // Test Helper Functions
  void SetOverlayState(gpu::Mailbox mailbox, bool overlay_state) {
    if (overlay_state_observer_.is_bound() && mailbox == mailbox_) {
      overlay_state_observer_->OnStateChanged(overlay_state);
    }
  }

 private:
  mojo::Remote<gpu::mojom::OverlayStateObserver> overlay_state_observer_;
  gpu::Mailbox mailbox_;
};

class OverlayStateObserverImplTest : public testing::Test {
 public:
  OverlayStateObserverImplTest() = default;
  ~OverlayStateObserverImplTest() override = default;

  void OnStateChanged(bool promoted) {
    promoted_ = promoted;
    ++callback_count_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockOverlayStateService mock_overlay_state_service;
  int callback_count_ = 0;
  bool promoted_ = false;
};

TEST_F(OverlayStateObserverImplTest, StateChange) {
  // Create a OverlayStateObserverImpl & register a mailbox to listen to
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  auto overlay_state_observer_subscription = OverlayStateObserverImpl::Create(
      &mock_overlay_state_service, mailbox,
      base::BindRepeating(&OverlayStateObserverImplTest::OnStateChanged,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(callback_count_, 0);

  // Set overlay state to false
  mock_overlay_state_service.SetOverlayState(mailbox, false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(callback_count_, 1);
  EXPECT_EQ(promoted_, false);

  // Set overlay state to true
  mock_overlay_state_service.SetOverlayState(mailbox, true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(promoted_, true);

  // Set overlay state for another mailbox & ensure no callback
  gpu::Mailbox mailbox2 = gpu::Mailbox::Generate();
  mock_overlay_state_service.SetOverlayState(mailbox2, false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(callback_count_, 2);
  EXPECT_EQ(promoted_, true);
}

}  // namespace content
