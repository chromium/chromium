// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <string>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/token.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "media/cdm/cdm_type.h"
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

ACTION_TEMPLATE(PostOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(std::get<k>(args)), p0));
}

class KeySystemSupportImplTest : public testing::Test {
 protected:
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
    key_system_capabilities["KeySystem"] = KeySystemCapability(
        std::move(sw_secure_capability), std::move(hw_secure_capability));
    return key_system_capabilities;
  }

  void OnIsKeySystemSupported(base::OnceClosure done_cb,
                              bool is_supported,
                              media::mojom::KeySystemCapabilityPtr capability) {
    is_supported_ = is_supported;
    capability_ = std::move(capability);
    std::move(done_cb).Run();
  }

  // Determines if |key_system| is registered. If it is, updates |codecs_|
  // and |persistent_|.
  bool IsSupported(const std::string& key_system) {
    DVLOG(1) << __func__;

    mojo::Remote<media::mojom::KeySystemSupport> key_system_support;
    KeySystemSupportImpl key_system_support_impl(get_support_cb_.Get());
    key_system_support_impl.Bind(
        key_system_support.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    key_system_support->IsKeySystemSupported(
        key_system,
        base::BindOnce(&KeySystemSupportImplTest::OnIsKeySystemSupported,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    return is_supported_;
  }

  // Same as `IsSupported()`, but calling into KeySystemSupportImpl directly
  // instead of using the mojo interface. This is to avoid the complication of
  // posted callbacks in async tests.
  bool IsSupportedWithoutMojo(const std::string& key_system) {
    DVLOG(1) << __func__;

    KeySystemSupportImpl key_system_support_impl(get_support_cb_.Get());

    base::RunLoop run_loop;
    key_system_support_impl.IsKeySystemSupported(
        key_system,
        base::BindOnce(&KeySystemSupportImplTest::OnIsKeySystemSupported,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    return is_supported_;
  }

  BrowserTaskEnvironment task_environment_;

  base::MockCallback<KeySystemSupportImpl::GetKeySystemCapabilitiesUpdateCB>
      get_support_cb_;

  // Updated by IsSupported().
  bool is_supported_ = false;
  media::mojom::KeySystemCapabilityPtr capability_;
};

TEST_F(KeySystemSupportImplTest, NoKeySystems) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(KeySystemCapabilities()));
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportImplTest, NoCapabilities) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(
          TestKeySystemCapabilities(absl::nullopt, absl::nullopt)));
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportImplTest, SoftwareSecureCapability_Sync) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(RunOnceCallback<0>(
          TestKeySystemCapabilities(TestCdmCapability(), absl::nullopt)));
  ASSERT_TRUE(IsSupported("KeySystem"));
  EXPECT_TRUE(capability_->sw_secure_capability);
  EXPECT_FALSE(capability_->hw_secure_capability);
}

// Same as above, but post the callback instead of running it directly, and uses
// `IsSupportedWithoutMojo`, to simulate the case where `CdmRegistryImpl`
// resolves the callback asynchronously.
TEST_F(KeySystemSupportImplTest, SoftwareSecureCapability_Async) {
  EXPECT_CALL(get_support_cb_, Run(_))
      .WillOnce(PostOnceCallback<0>(
          TestKeySystemCapabilities(TestCdmCapability(), absl::nullopt)));
  ASSERT_TRUE(IsSupportedWithoutMojo("KeySystem"));
  EXPECT_TRUE(capability_->sw_secure_capability);
  EXPECT_FALSE(capability_->hw_secure_capability);
}

}  // namespace content
