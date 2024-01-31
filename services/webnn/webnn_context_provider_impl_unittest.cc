// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/remote.h"
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
// For Windows platform, `dml::ContextImpl` is implemented by the DirectML
// backend. It relies on a real GPU adapter and is tested by
// `WebNNContextDMLImplTest`.

#if !BUILDFLAG(IS_WIN)

TEST_F(WebNNContextProviderImplTest, CreateWebNNContextTest) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSVersion() >= 13'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is supported on Mac OS "
                 << base::mac::MacOSVersion();
  }
#endif  // BUILDFLAG(IS_MAC)

  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::CreateContextResultPtr> future;
  provider_remote->CreateWebNNContext(mojom::CreateContextOptions::New(),
                                      future.GetCallback());
  mojom::CreateContextResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  const auto& create_context_error = result->get_error();
  EXPECT_EQ(create_context_error->code, mojom::Error::Code::kNotSupportedError);
  EXPECT_EQ(create_context_error->message,
            "WebNN Service is not supported on this platform.");
}

#endif

}  // namespace webnn
