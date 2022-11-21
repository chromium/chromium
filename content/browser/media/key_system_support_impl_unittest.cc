// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using AudioCodec = media::AudioCodec;
using VideoCodec = media::VideoCodec;
using EncryptionScheme = media::EncryptionScheme;
using CdmSessionType = media::CdmSessionType;
using Robustness = CdmInfo::Robustness;
using base::test::RunOnceCallback;
using media::CdmCapability;
using media::mojom::KeySystemCapability;
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

using KeySystemSupportCB =
    base::RepeatingCallback<void(KeySystemCapabilityPtrMap)>;

class KeySystemSupportObserverImpl
    : public media::mojom::KeySystemSupportObserver {
 public:
  explicit KeySystemSupportObserverImpl(KeySystemSupportCB cb)
      : key_system_support_cb_(std::move(cb)) {}
  KeySystemSupportObserverImpl(const KeySystemSupportObserverImpl&) = delete;
  KeySystemSupportObserverImpl& operator=(const KeySystemSupportObserverImpl&) =
      delete;
  ~KeySystemSupportObserverImpl() override = default;

  // media::mojom::KeySystemSupportObserver
  void OnKeySystemSupportUpdated(KeySystemCapabilityPtrMap capabilities) final {
    key_system_support_cb_.Run(std::move(capabilities));
  }

 private:
  KeySystemSupportCB key_system_support_cb_;
};

CdmCapability TestCdmCapability() {
  return CdmCapability(
      {AudioCodec::kVorbis}, {{VideoCodec::kVP8, {}}, {VideoCodec::kVP9, {}}},
      {EncryptionScheme::kCenc, EncryptionScheme::kCbcs},
      {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
}

KeySystemCapabilities TestKeySystemCapabilities(
    absl::optional<CdmCapability> sw_secure_capability,
    absl::optional<CdmCapability> hw_secure_capability) {
  KeySystemCapabilities key_system_capabilities;
  key_system_capabilities[kTestKeySystem] = KeySystemCapability(
      std::move(sw_secure_capability), std::move(hw_secure_capability));
  return key_system_capabilities;
}

}  // namespace

class KeySystemSupportImplTest : public testing::Test {
 public:
  KeySystemSupportImplTest() {
    LOG(ERROR) << __func__;
    key_system_support_impl_.SetGetKeySystemCapabilitiesUpdateCbForTesting(
        get_support_cb_.Get());
    key_system_support_impl_.Bind(
        key_system_support_.BindNewPipeAndPassReceiver());
  }

  void OnKeySystemSupportUpdated(int observer_id,
                                 base::OnceClosure done_cb,
                                 KeySystemCapabilityPtrMap capabilities) {
    results_[observer_id].push_back(std::move(capabilities));
    std::move(done_cb).Run();
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

  BrowserTaskEnvironment task_environment_;
  KeySystemSupportImpl key_system_support_impl_;
  mojo::Remote<media::mojom::KeySystemSupport> key_system_support_;
  base::MockCallback<KeySystemSupportImpl::GetKeySystemCapabilitiesUpdateCB>
      get_support_cb_;

  // KeySystemSupport update results. It's a map from the "observer ID" to the
  // list of updates received by that observer.
  std::map<int, std::vector<KeySystemCapabilityPtrMap>> results_;
};

TEST_F(KeySystemSupportImplTest, NoKeySystems) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(KeySystemCapabilities()));
  GetKeySystemSupport();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 1u);  // One update for observer 1

  const auto& capabilities = results_[kObserver1][0];
  EXPECT_TRUE(capabilities.empty());  // No capabilities
}

TEST_F(KeySystemSupportImplTest, OneObserver) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(
          TestKeySystemCapabilities(TestCdmCapability(), absl::nullopt)));
  GetKeySystemSupport();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 1u);  // One update for observer 1

  auto& capabilities = results_[kObserver1][0];
  ASSERT_TRUE(capabilities.count(kTestKeySystem));
  const auto& capability = capabilities[kTestKeySystem];
  EXPECT_TRUE(capability->sw_secure_capability);
  EXPECT_FALSE(capability->hw_secure_capability);
}

TEST_F(KeySystemSupportImplTest, TwoObservers) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(
          TestKeySystemCapabilities(TestCdmCapability(), absl::nullopt)));

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
  EXPECT_TRUE(capability->sw_secure_capability);
  EXPECT_FALSE(capability->hw_secure_capability);

  EXPECT_TRUE(results_.count(kObserver2));     // Observer 2
  EXPECT_EQ(results_[kObserver2].size(), 1u);  // One update for observer 1
  EXPECT_TRUE(mojo::Equals(results_[kObserver1][0], results_[kObserver2][0]));
}

TEST_F(KeySystemSupportImplTest, TwoUpdates) {
  KeySystemCapabilitiesUpdateCB callback;
  EXPECT_CALL(get_support_cb_, Run(_)).WillOnce(SaveArg<0>(&callback));

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
  callback.Run(TestKeySystemCapabilities(TestCdmCapability(), absl::nullopt));
  callback.Run(
      TestKeySystemCapabilities(TestCdmCapability(), TestCdmCapability()));
  run_loop_2.RunUntilIdle();

  EXPECT_EQ(results_.size(), 1u);              // One observer
  EXPECT_TRUE(results_.count(kObserver1));     // Observer 1
  EXPECT_EQ(results_[kObserver1].size(), 2u);  // Two updates for observer 1

  auto& capabilities_1 = results_[kObserver1][0];
  ASSERT_TRUE(capabilities_1.count(kTestKeySystem));
  const auto& capability_1 = capabilities_1[kTestKeySystem];
  EXPECT_TRUE(capability_1->sw_secure_capability);
  EXPECT_FALSE(capability_1->hw_secure_capability);

  auto& capabilities_2 = results_[kObserver1][1];
  ASSERT_TRUE(capabilities_2.count(kTestKeySystem));
  const auto& capability_2 = capabilities_2[kTestKeySystem];
  EXPECT_TRUE(capability_2->sw_secure_capability);
  EXPECT_TRUE(capability_2->hw_secure_capability);
}

}  // namespace content
