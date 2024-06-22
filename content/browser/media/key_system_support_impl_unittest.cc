// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/cdm_capability.h"
#include "media/base/key_system_capability.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace content {

using AudioCodec = media::AudioCodec;
using VideoCodec = media::VideoCodec;
using EncryptionScheme = media::EncryptionScheme;
using CdmSessionType = media::CdmSessionType;
using Robustness = CdmInfo::Robustness;
using base::test::RunOnceCallback;
using media::CdmCapability;
using media::KeySystemCapability;
using testing::_;
using testing::SaveArg;

const char kTestKeySystem[] = "com.example.somesystem";

// Ids to keep track of observers.
const int kObserver1 = 1;
const int kObserver2 = 2;

ACTION_TEMPLATE(PostOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(std::get<k>(args)), p0));
}

namespace {

class KeySystemSupportObserverImpl
    : public media::mojom::KeySystemSupportObserver {
 public:
  explicit KeySystemSupportObserverImpl(KeySystemCapabilitiesUpdateCB cb)
      : key_system_support_cb_(std::move(cb)) {}
  KeySystemSupportObserverImpl(const KeySystemSupportObserverImpl&) = delete;
  KeySystemSupportObserverImpl& operator=(const KeySystemSupportObserverImpl&) =
      delete;
  ~KeySystemSupportObserverImpl() override = default;

  // media::mojom::KeySystemSupportObserver
  void OnKeySystemSupportUpdated(
      const KeySystemCapabilities& capabilities) final {
    key_system_support_cb_.Run(std::move(capabilities));
  }

 private:
  KeySystemCapabilitiesUpdateCB key_system_support_cb_;
};

CdmCapability TestCdmCapability() {
  return CdmCapability(
      {AudioCodec::kVorbis}, {{VideoCodec::kVP8, {}}, {VideoCodec::kVP9, {}}},
      {EncryptionScheme::kCenc, EncryptionScheme::kCbcs},
      {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
}

KeySystemCapabilities TestKeySystemCapabilities(
    std::optional<CdmCapability> sw_secure_capability,
    std::optional<CdmCapability> hw_secure_capability) {
  KeySystemCapabilities key_system_capabilities;
  key_system_capabilities[kTestKeySystem] = KeySystemCapability(
      std::move(sw_secure_capability), std::move(hw_secure_capability));
  return key_system_capabilities;
}

}  // namespace

class KeySystemSupportImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    LOG(ERROR) << __func__;
    RenderViewHostTestHarness::SetUp();

    test_browser_context_ = std::make_unique<content::TestBrowserContext>();
    test_browser_context_->SetPermissionControllerForTesting(
        std::make_unique<testing::NiceMock<MockPermissionController>>());

    KeySystemSupportImpl::GetOrCreateForCurrentDocument(main_rfh())
        ->SetGetKeySystemCapabilitiesUpdateCbForTesting(get_support_cb_.Get());
    KeySystemSupportImpl::GetOrCreateForCurrentDocument(main_rfh())
        ->Bind(key_system_support_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    test_browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void OnKeySystemSupportUpdated(int observer_id,
                                 base::OnceClosure done_cb,
                                 KeySystemCapabilities capabilities) {
    results_[observer_id].push_back(std::move(capabilities));
    std::move(done_cb).Run();
  }

  void SetPermissionStatus(blink::mojom::PermissionStatus permission_status) {
    auto* mock_permission_controller = static_cast<MockPermissionController*>(
        test_browser_context_->GetPermissionController());

    ON_CALL(*mock_permission_controller,
            RequestPermissionFromCurrentDocument(
                main_rfh(),
                PermissionRequestDescription(
                    blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                    main_rfh()->HasTransientUserActivation()),
                _))
        .WillByDefault(RunOnceCallback<2>(permission_status));

    KeySystemSupportImpl::GetForCurrentDocument(main_rfh())
        ->OnProtectedMediaIdentifierPermissionUpdated(permission_status);
  }

 protected:
  void GetKeySystemSupport() {
    DVLOG(1) << __func__;

    base::RunLoop run_loop;
    mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<KeySystemSupportObserverImpl>(base::BindRepeating(
            &KeySystemSupportImplTest::OnKeySystemSupportUpdated,
            base::Unretained(this), kObserver1, run_loop.QuitClosure())),
        observer_remote.InitWithNewPipeAndPassReceiver());
    key_system_support_->AddObserver(std::move(observer_remote));
    run_loop.Run();
  }

  mojo::Remote<media::mojom::KeySystemSupport> key_system_support_;
  base::MockCallback<KeySystemSupportImpl::GetKeySystemCapabilitiesUpdateCB>
      get_support_cb_;

  // KeySystemSupport update results. It's a map from the "observer ID" to the
  // list of updates received by that observer.
  std::map<int, std::vector<KeySystemCapabilities>> results_;
  std::unique_ptr<content::TestBrowserContext> test_browser_context_;
};

TEST_F(KeySystemSupportImplTest, NoKeySystems) {
  EXPECT_CALL(get_support_cb_, Run(_, _))
      .WillOnce(RunOnceCallback<1>(KeySystemCapabilities()));
  GetKeySystemSupport();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 1u);  // One update for observer 1

  const auto& capabilities = results_[kObserver1][0];
  EXPECT_TRUE(capabilities.empty());  // No capabilities
}

TEST_F(KeySystemSupportImplTest, OneObserver) {
  EXPECT_CALL(get_support_cb_, Run(_, _))
      .WillOnce(RunOnceCallback<1>(
          TestKeySystemCapabilities(TestCdmCapability(), std::nullopt)));
  GetKeySystemSupport();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 1u);  // One update for observer 1

  auto& capabilities = results_[kObserver1][0];
  ASSERT_TRUE(capabilities.count(kTestKeySystem));
  const auto& capability = capabilities[kTestKeySystem];
  EXPECT_TRUE(capability.sw_secure_capability);
  EXPECT_FALSE(capability.hw_secure_capability);
}

TEST_F(KeySystemSupportImplTest, TwoObservers) {
  EXPECT_CALL(get_support_cb_, Run(_, _))
      .WillOnce(RunOnceCallback<1>(
          TestKeySystemCapabilities(TestCdmCapability(), std::nullopt)));

  base::RunLoop run_loop;
  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_1_remote;
  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_2_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<KeySystemSupportObserverImpl>(base::BindRepeating(
          &KeySystemSupportImplTest::OnKeySystemSupportUpdated,
          base::Unretained(this), kObserver1, base::DoNothing())),
      observer_1_remote.InitWithNewPipeAndPassReceiver());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<KeySystemSupportObserverImpl>(base::BindRepeating(
          &KeySystemSupportImplTest::OnKeySystemSupportUpdated,
          base::Unretained(this), kObserver2, run_loop.QuitClosure())),
      observer_2_remote.InitWithNewPipeAndPassReceiver());
  key_system_support_->AddObserver(std::move(observer_1_remote));
  key_system_support_->AddObserver(std::move(observer_2_remote));
  run_loop.Run();

  EXPECT_EQ(results_.size(), 2u);  // Two observers

  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 1u);  // One update for observer 1
  auto& capabilities = results_[kObserver1][0];
  ASSERT_TRUE(capabilities.count(kTestKeySystem));
  const auto& capability = capabilities[kTestKeySystem];
  EXPECT_TRUE(capability.sw_secure_capability);
  EXPECT_FALSE(capability.hw_secure_capability);

  EXPECT_TRUE(results_.count(kObserver2));     // Observer 2
  EXPECT_EQ(results_[kObserver2].size(), 1u);  // One update for observer 1
  EXPECT_TRUE(mojo::Equals(results_[kObserver1][0], results_[kObserver2][0]));
}

TEST_F(KeySystemSupportImplTest, TwoUpdates) {
  KeySystemCapabilitiesUpdateCB callback;
  EXPECT_CALL(get_support_cb_, Run(_, _)).WillOnce(SaveArg<1>(&callback));

  base::RunLoop run_loop_1;
  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<KeySystemSupportObserverImpl>(base::BindRepeating(
          &KeySystemSupportImplTest::OnKeySystemSupportUpdated,
          base::Unretained(this), kObserver1, base::DoNothing())),
      observer_remote.InitWithNewPipeAndPassReceiver());
  key_system_support_->AddObserver(std::move(observer_remote));
  run_loop_1.RunUntilIdle();

  // Update twice, one with hardware capability, one without.
  base::RunLoop run_loop_2;
  callback.Run(TestKeySystemCapabilities(TestCdmCapability(), std::nullopt));
  callback.Run(
      TestKeySystemCapabilities(TestCdmCapability(), TestCdmCapability()));
  run_loop_2.RunUntilIdle();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 2u);  // Two updates for observer 1

  auto& capabilities_1 = results_[kObserver1][0];
  ASSERT_TRUE(capabilities_1.count(kTestKeySystem));
  const auto& capability_1 = capabilities_1[kTestKeySystem];
  EXPECT_TRUE(capability_1.sw_secure_capability);
  EXPECT_FALSE(capability_1.hw_secure_capability);

  auto& capabilities_2 = results_[kObserver1][1];
  ASSERT_TRUE(capabilities_2.count(kTestKeySystem));
  const auto& capability_2 = capabilities_2[kTestKeySystem];
  EXPECT_TRUE(capability_2.sw_secure_capability);
  EXPECT_TRUE(capability_2.hw_secure_capability);
}

TEST_F(KeySystemSupportImplTest, AllowHWSecureCapability) {
  base::RunLoop run_loop_1;
  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<KeySystemSupportObserverImpl>(base::BindRepeating(
          &KeySystemSupportImplTest::OnKeySystemSupportUpdated,
          base::Unretained(this), kObserver1, base::DoNothing())),
      observer_remote.InitWithNewPipeAndPassReceiver());
  key_system_support_->AddObserver(std::move(observer_remote));

// Only windows can disallow hw secure capability.
#if BUILDFLAG(IS_WIN)
  EXPECT_CALL(get_support_cb_, Run(testing::IsFalse(), _))
      .WillOnce(RunOnceCallback<1>(KeySystemCapabilities()));
#else
  EXPECT_CALL(get_support_cb_, Run(testing::IsTrue(), _))
      .WillOnce(RunOnceCallback<1>(KeySystemCapabilities()));
#endif

  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);
  run_loop_1.RunUntilIdle();

  // Switching the permission to GRANTED should change hw secure capability.
  base::RunLoop run_loop_2;
  EXPECT_CALL(get_support_cb_, Run(testing::IsTrue(), _))
      .WillOnce(RunOnceCallback<1>(KeySystemCapabilities()));
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  run_loop_2.RunUntilIdle();
}

}  // namespace content
