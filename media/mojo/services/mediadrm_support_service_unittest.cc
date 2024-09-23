// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mediadrm_support_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/mojo/mojom/mediadrm_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

namespace {

class MediaDrmSupportServiceTest : public testing::Test {
 public:
  void Initialize() {
    service_ = std::make_unique<MediaDrmSupportService>(
        remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaDrmSupportService> service_;
  mojo::Remote<mojom::MediaDrmSupport> remote_;
};

}  // namespace

TEST_F(MediaDrmSupportServiceTest, WidevineKeySystem) {
  Initialize();

  base::test::TestFuture<mojom::MediaDrmSupportResultPtr> support;
  service_->IsKeySystemSupported(kWidevineKeySystem, support.GetCallback());

  // All Android devices should support some form of Widevine support.
  // However, support for WebM and MP4 formats may vary between devices,
  // so we cannot check the other parameters.
  ASSERT_TRUE(support.Get());
}

TEST_F(MediaDrmSupportServiceTest, UnknownKeySystem) {
  const char kUnsupportedKeySystem[] = "keysystem.test.unsupported";

  Initialize();

  base::test::TestFuture<mojom::MediaDrmSupportResultPtr> support;
  service_->IsKeySystemSupported(kUnsupportedKeySystem, support.GetCallback());

  // Unknown key system should not be supported.
  ASSERT_FALSE(support.Get());
}

}  // namespace media
