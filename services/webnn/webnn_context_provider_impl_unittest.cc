// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace webnn {

class WebNNContextProviderImplTest : public testing::Test {
 public:
  WebNNContextProviderImplTest(const WebNNContextProviderImplTest&) = delete;
  WebNNContextProviderImplTest& operator=(const WebNNContextProviderImplTest&) =
      delete;

 protected:
  WebNNContextProviderImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNContextProviderImplTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

// `WebNNContextProviderImplTest` only focuses on the non-supported platforms.
// For supported platforms, it should be tested by the backend specific test
// cases.
//
// For Windows platform, `dml::ContextImplDml` is implemented by the DirectML
// backend. It relies on a real GPU adapter and is tested by
// `WebNNContextDMLImplTest`.
//
// For platforms using TFLite, `tflite::ContextImplTflite` is always available.

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(WEBNN_USE_TFLITE)

TEST_F(WebNNContextProviderImplTest, NotSupported) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSVersion() >= 13'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is supported on Mac OS "
                 << base::mac::MacOSVersion();
  }
#endif  // BUILDFLAG(IS_MAC)

  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(mojom::CreateContextOptions::New(),
                                      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::ErrorPtr& create_context_error = result->get_error();
  EXPECT_EQ(create_context_error->code, mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(create_context_error->message,
            "WebNN Service is not supported on this platform.");
}

#endif

#if BUILDFLAG(IS_WIN)

TEST_F(WebNNContextProviderImplTest, CPUIsSupported) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kCpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_success());
  EXPECT_TRUE(result->get_success()->context_remote.is_valid());
}

// Checking for GPU/NPU compatibility is Windows-specific because only the
// DirectML implementation unconditionally depends on a GPU/NPU.

TEST_F(WebNNContextProviderImplTest, GPUNotSupported) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNGpuDisabled);

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kGpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::ErrorPtr& create_context_error = result->get_error();
  EXPECT_EQ(create_context_error->code, mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(create_context_error->message,
            "DirectML: WebNN is blocklisted for GPU.");
}

TEST_F(WebNNContextProviderImplTest, NPUNotSupported) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNNpuDisabled);

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kNpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::ErrorPtr& create_context_error = result->get_error();
  EXPECT_EQ(create_context_error->code, mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(create_context_error->message,
            "DirectML: WebNN is blocklisted for NPU.");
}

TEST_F(WebNNContextProviderImplTest, GpuFeatureStatusDisabled) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver(),
      WebNNContextProviderImpl::WebNNStatus::kWebNNGpuFeatureStatusDisabled);

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kNpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::ErrorPtr& create_context_error = result->get_error();
  EXPECT_EQ(create_context_error->code, mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(create_context_error->message,
            "WebNN is not compatible with device.");
}

#endif

}  // namespace webnn
