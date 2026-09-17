// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/periodic_background_sync_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/background_sync/background_sync_service_impl_test_harness.h"
#include "content/public/test/mock_render_process_host.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class PeriodicBackgroundSyncServiceImplTest
    : public BackgroundSyncServiceImplTestHarness {
 public:
  PeriodicBackgroundSyncServiceImplTest() = default;

  void SetUp() override {
    BackgroundSyncServiceImplTestHarness::SetUp();
    CreatePeriodicBackgroundSyncServiceImpl();
  }
  void TearDown() override {
    periodic_sync_service_impl_ = nullptr;
    BackgroundSyncServiceImplTestHarness::TearDown();
  }

 protected:
  void CreatePeriodicBackgroundSyncServiceImpl() {
    render_process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context());

    // Create a dummy mojo channel so that the PeriodicBackgroundSyncServiceImpl
    // can be instantiated.
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver = periodic_sync_service_remote_.BindNewPipeAndPassReceiver();
    // Create a new PeriodicBackgroundSyncServiceImpl bound to the dummy
    // channel.
    background_sync_context_->CreatePeriodicSyncService(
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(GURL(kServiceWorkerOrigin))),
        render_process_host_.get(), std::move(receiver));
    base::RunLoop().RunUntilIdle();

    // Since |background_sync_context_| is deleted after
    // PeriodicBackgroundSyncServiceImplTest is, this is safe.
    periodic_sync_service_impl_ =
        background_sync_context_->periodic_sync_services_.begin()->get();
    ASSERT_TRUE(periodic_sync_service_impl_);
  }

  // Helpers for testing *BackgroundSyncServiceImpl methods
  void RegisterPeriodicSync(
      blink::mojom::SyncRegistrationOptionsPtr sync,
      blink::mojom::PeriodicBackgroundSyncService::RegisterCallback callback) {
    periodic_sync_service_impl_->Register(std::move(sync), sw_registration_id_,
                                          std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void UnregisterPeriodicSync(
      const std::string& tag,
      blink::mojom::PeriodicBackgroundSyncService::UnregisterCallback
          callback) {
    periodic_sync_service_impl_->Unregister(sw_registration_id_, tag,
                                            std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void GetPeriodicSyncRegistrations(
      blink::mojom::PeriodicBackgroundSyncService::GetRegistrationsCallback
          callback) {
    periodic_sync_service_impl_->GetRegistrations(sw_registration_id_,
                                                  std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  mojo::Remote<blink::mojom::PeriodicBackgroundSyncService>
      periodic_sync_service_remote_;

  // Owned by |background_sync_context_|
  raw_ptr<PeriodicBackgroundSyncServiceImpl> periodic_sync_service_impl_;
};

// Tests

TEST_F(PeriodicBackgroundSyncServiceImplTest, RegisterPeriodicSync) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationOptionsPtr reg;
  auto to_register = default_sync_registration_.Clone();
  to_register->min_interval = 3600;
  RegisterPeriodicSync(
      std::move(to_register),
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  ASSERT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ("", reg->tag);
  EXPECT_EQ(3600, reg->min_interval);
}

TEST_F(PeriodicBackgroundSyncServiceImplTest, RegisterWithInvalidMinInterval) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationOptionsPtr reg;
  auto to_register = default_sync_registration_.Clone();
  to_register->min_interval = -1;

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  RegisterPeriodicSync(
      std::move(to_register),
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  ASSERT_TRUE(called);
  EXPECT_EQ(mojo_bad_messages_.size(), 1u);
}

TEST_F(PeriodicBackgroundSyncServiceImplTest,
       GetPeriodicSyncRegistrationsNoSyncRegistered) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  unsigned long array_size = 0UL;
  GetPeriodicSyncRegistrations(base::BindOnce(&ErrorAndRegistrationListCallback,
                                              &called, &error, &array_size));
  ASSERT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(PeriodicBackgroundSyncServiceImplTest,
       GetPeriodicSyncRegistrationsWithRegisteredSync) {
  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;
    blink::mojom::SyncRegistrationOptionsPtr registered_reg;

    auto to_register = default_sync_registration_.Clone();
    to_register->min_interval = 3600;

    RegisterPeriodicSync(std::move(to_register),
                         base::BindOnce(&ErrorAndRegistrationCallback, &called,
                                        &error, &registered_reg));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  }

  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;
    unsigned long array_size = 0UL;
    GetPeriodicSyncRegistrations(base::BindOnce(
        &ErrorAndRegistrationListCallback, &called, &error, &array_size));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
    EXPECT_EQ(1UL, array_size);
  }
}

TEST_F(PeriodicBackgroundSyncServiceImplTest, Unregister) {
  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;

    UnregisterPeriodicSync("non_existent",
                           base::BindOnce(&ErrorCallback, &called, &error));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  }

  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;
    blink::mojom::SyncRegistrationOptionsPtr reg;
    auto to_register = default_sync_registration_.Clone();
    to_register->tag = "shared_tag";
    to_register->min_interval = 3600;
    RegisterPeriodicSync(
        std::move(to_register),
        base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
    EXPECT_EQ("shared_tag", reg->tag);
    EXPECT_EQ(3600, reg->min_interval);
  }
  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;

    UnregisterPeriodicSync("shared_tag",
                           base::BindOnce(&ErrorCallback, &called, &error));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  }
  {
    bool called = false;
    blink::mojom::BackgroundSyncError error;
    unsigned long array_size = 0UL;
    GetPeriodicSyncRegistrations(base::BindOnce(
        &ErrorAndRegistrationListCallback, &called, &error, &array_size));
    ASSERT_TRUE(called);
    EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
    EXPECT_EQ(0UL, array_size);
  }
}

TEST_F(PeriodicBackgroundSyncServiceImplTest,
       RegisterPeriodicSync_DifferentStorageKey) {
  mojo::Remote<blink::mojom::PeriodicBackgroundSyncService> remote;
  background_sync_context_->CreatePeriodicSyncService(
      blink::StorageKey::Create(url::Origin::Create(GURL(kServiceWorkerOrigin)),
                                net::SchemefulSite(GURL("https://other.com")),
                                blink::mojom::AncestorChainBit::kCrossSite),
      render_process_host_.get(), remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationOptionsPtr reg;
  auto to_register = default_sync_registration_.Clone();
  to_register->min_interval = 3600;
  remote->Register(
      std::move(to_register), sw_registration_id_,
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::STORAGE, error);
  EXPECT_FALSE(reg);
}

TEST_F(PeriodicBackgroundSyncServiceImplTest,
       GetPeriodicSyncRegistrations_DifferentStorageKey) {
  mojo::Remote<blink::mojom::PeriodicBackgroundSyncService> remote;
  background_sync_context_->CreatePeriodicSyncService(
      blink::StorageKey::Create(url::Origin::Create(GURL(kServiceWorkerOrigin)),
                                net::SchemefulSite(GURL("https://other.com")),
                                blink::mojom::AncestorChainBit::kCrossSite),
      render_process_host_.get(), remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  bool called = false;
  blink::mojom::BackgroundSyncError error;
  unsigned long array_size = 0UL;
  remote->GetRegistrations(sw_registration_id_,
                           base::BindOnce(&ErrorAndRegistrationListCallback,
                                          &called, &error, &array_size));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::STORAGE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(PeriodicBackgroundSyncServiceImplTest, Unregister_DifferentStorageKey) {
  mojo::Remote<blink::mojom::PeriodicBackgroundSyncService> remote;
  background_sync_context_->CreatePeriodicSyncService(
      blink::StorageKey::Create(url::Origin::Create(GURL(kServiceWorkerOrigin)),
                                net::SchemefulSite(GURL("https://other.com")),
                                blink::mojom::AncestorChainBit::kCrossSite),
      render_process_host_.get(), remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  bool called = false;
  blink::mojom::BackgroundSyncError error;
  remote->Unregister(sw_registration_id_, "shared_tag",
                     base::BindOnce(&ErrorCallback, &called, &error));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::STORAGE, error);
}

}  // namespace content
