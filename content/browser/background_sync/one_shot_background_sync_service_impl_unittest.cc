// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/one_shot_background_sync_service_impl.h"

#include "content/browser/background_sync/background_sync_service_impl_test_harness.h"

namespace content {

class OneShotBackgroundSyncServiceImplTest
    : public BackgroundSyncServiceImplTestHarness {
 public:
  OneShotBackgroundSyncServiceImplTest() = default;
  ~OneShotBackgroundSyncServiceImplTest() override = default;

  void SetUp() override {
    BackgroundSyncServiceImplTestHarness::SetUp();
    CreateOneShotBackgroundSyncServiceImpl();
  }

 protected:
  void CreateOneShotBackgroundSyncServiceImpl() {
    // Create a dummy mojo channel so that the OneShotBackgroundSyncServiceImpl
    // can be instantiated.
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService> receiver =
        one_shot_sync_service_remote_.BindNewPipeAndPassReceiver();
    // Create a new OneShotBackgroundSyncServiceImpl bound to the dummy channel.
    background_sync_context_->CreateOneShotSyncService(std::move(receiver));
    base::RunLoop().RunUntilIdle();

    // Since |background_sync_context_| is deleted after
    // OneShotBackgroundSyncServiceImplTest is, this is safe.
    one_shot_sync_service_impl_ =
        background_sync_context_->one_shot_sync_services_.begin()->get();
    ASSERT_TRUE(one_shot_sync_service_impl_);
  }

  // Helpers for testing *BackgroundSyncServiceImpl methods
  void RegisterOneShotSync(
      blink::mojom::SyncRegistrationOptionsPtr sync,
      blink::mojom::OneShotBackgroundSyncService::RegisterCallback callback) {
    one_shot_sync_service_impl_->Register(std::move(sync), sw_registration_id_,
                                          std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void GetOneShotSyncRegistrations(
      blink::mojom::OneShotBackgroundSyncService::GetRegistrationsCallback
          callback) {
    one_shot_sync_service_impl_->GetRegistrations(sw_registration_id_,
                                                  std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  mojo::Remote<blink::mojom::OneShotBackgroundSyncService>
      one_shot_sync_service_remote_;

  // Owned by |background_sync_context_|
  OneShotBackgroundSyncServiceImpl* one_shot_sync_service_impl_;
};

// Tests

TEST_F(OneShotBackgroundSyncServiceImplTest, RegisterOneShotSync) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationOptionsPtr reg;
  RegisterOneShotSync(
      default_sync_registration_.Clone(),
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  ASSERT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ("", reg->tag);
}

TEST_F(OneShotBackgroundSyncServiceImplTest, RegisterWithInvalidOptions) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationOptionsPtr reg;
  auto to_register = default_sync_registration_.Clone();
  to_register->min_interval = 3600;

  FakeMojoMessageDispatchContext fake_dispatch_context;
  RegisterOneShotSync(
      std::move(to_register),
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  ASSERT_TRUE(called);
  EXPECT_EQ(mojo_bad_messages_.size(), 1u);
}

TEST_F(OneShotBackgroundSyncServiceImplTest,
       GetOneShotSyncRegistrationsNoSyncRegistered) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  unsigned long array_size = 0UL;
  GetOneShotSyncRegistrations(base::BindOnce(&ErrorAndRegistrationListCallback,
                                             &called, &error, &array_size));
  ASSERT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(OneShotBackgroundSyncServiceImplTest,
       GetOneShotSyncRegistrationsWithRegisteredSync) {
  bool register_called = false;
  bool get_registrations_called = false;
  blink::mojom::BackgroundSyncError register_error;
  blink::mojom::BackgroundSyncError get_registrations_error;
  blink::mojom::SyncRegistrationOptionsPtr registered_reg;
  unsigned long array_size = 0UL;
  RegisterOneShotSync(
      default_sync_registration_.Clone(),
      base::BindOnce(&ErrorAndRegistrationCallback, &register_called,
                     &register_error, &registered_reg));
  ASSERT_TRUE(register_called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, register_error);
  GetOneShotSyncRegistrations(base::BindOnce(
      &ErrorAndRegistrationListCallback, &get_registrations_called,
      &get_registrations_error, &array_size));
  ASSERT_TRUE(get_registrations_called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, get_registrations_error);
  EXPECT_EQ(1UL, array_size);
}

}  // namespace content
