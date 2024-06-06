// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/test_base.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace webnn::test {

namespace {

class BadMessageTestHelper {
 public:
  BadMessageTestHelper() {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BadMessageTestHelper::OnBadMessage, base::Unretained(this)));
  }

  ~BadMessageTestHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  const std::optional<std::string>& GetLastBadMessage() const {
    return last_bad_message_report_;
  }

 private:
  void OnBadMessage(const std::string& reason) {
    ASSERT_FALSE(last_bad_message_report_.has_value());
    last_bad_message_report_ = reason;
  }

  std::optional<std::string> last_bad_message_report_;
};

#if BUILDFLAG(IS_WIN)
class WebNNBufferImplBackendTest : public dml::TestBase {
 public:
  WebNNBufferImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetUp() override;
  void TearDown() override;

 protected:
  bool CreateWebNNContext(
      mojo::Remote<mojom::WebNNContext>& webnn_context_remote);

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<dml::Adapter> adapter_;
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote_;
};

void WebNNBufferImplBackendTest::SetUp() {
  SKIP_TEST_IF(!dml::UseGPUInTests());

  dml::Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = dml::Adapter::GetInstanceForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!adapter_creation_result.has_value());
  adapter_ = adapter_creation_result.value();
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter_->IsDMLDeviceCompileGraphSupportedForTesting());

  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());
}
#elif BUILDFLAG(IS_MAC)
class WebNNBufferImplBackendTest : public testing::Test {
 public:
  WebNNBufferImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetUp() override;
  void TearDown() override;

 protected:
  bool CreateWebNNContext(
      mojo::Remote<mojom::WebNNContext>& webnn_context_remote);

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote_;
};

void WebNNBufferImplBackendTest::SetUp() {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }

  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());

  GTEST_SKIP() << "WebNNBuffer not implemented on macOS";
}
#elif BUILDFLAG(WEBNN_USE_TFLITE)
class WebNNBufferImplBackendTest : public testing::Test {
 public:
  WebNNBufferImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
#endif

    WebNNContextProviderImpl::CreateForTesting(
        webnn_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override;

 protected:
  bool CreateWebNNContext(
      mojo::Remote<mojom::WebNNContext>& webnn_context_remote);

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote_;
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
#endif
};
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

void WebNNBufferImplBackendTest::TearDown() {
  webnn_provider_remote_.reset();
  base::RunLoop().RunUntilIdle();
}

bool WebNNBufferImplBackendTest::CreateWebNNContext(
    mojo::Remote<mojom::WebNNContext>& webnn_context_remote) {
  bool is_platform_supported = true;

  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote_->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kGpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      create_context_future.GetCallback());
  auto create_context_result = create_context_future.Take();
  if (create_context_result->is_success()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_success()->context_remote));
  } else {
    is_platform_supported = create_context_result->get_error()->code !=
                            mojom::Error::Code::kNotSupportedError;
  }
  return is_platform_supported;
}

bool IsBufferDataEqual(const mojo_base::BigBuffer& a,
                       const mojo_base::BigBuffer& b) {
  return base::span(a) == base::span(b);
}

TEST_F(WebNNBufferImplBackendTest, CreateBufferImplTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  ASSERT_TRUE(webnn_context_remote.is_bound());

  constexpr uint64_t kBufferSize = 4ull;

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Test creating an over-sized WebNNBuffer should always fail.
TEST_F(WebNNBufferImplBackendTest, CreateBufferImplOversizedTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  ASSERT_TRUE(webnn_context_remote.is_bound());

  constexpr uint64_t kBufferSizeTooLarge = std::numeric_limits<uint64_t>::max();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSizeTooLarge),
      base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Creating two or more WebNNBuffer(s) with separate tokens should always
// succeed.
TEST_F(WebNNBufferImplBackendTest, CreateBufferImplManyTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  constexpr uint64_t kBufferSize = 4ull;

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_1.is_bound());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_2.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Creating two or more WebNNBuffer(s) with the same token should always fail.
TEST_F(WebNNBufferImplBackendTest, CreateBufferImplManySameTokenTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  constexpr uint64_t kBufferSize = 4ull;

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Disconnecting a WebNNBuffer should allow another buffer to be created with
// the same token.
TEST_F(WebNNBufferImplBackendTest,
       CreateBufferImplManyReuseTokenAfterDisconnectTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  constexpr uint64_t kBufferSize = 4ull;

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_buffer_remote_1.reset();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_3;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_3.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// TODO(https://crbug.com/40278771): Test the buffer gets destroyed.

TEST_F(WebNNBufferImplBackendTest, WriteBufferImplTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(4ull), base::UnguessableToken::Create());

  const std::array<const uint8_t, 4> input_data{0xAA, 0xAA, 0xAA, 0xAA};
  webnn_buffer_remote->WriteBuffer(mojo_base::BigBuffer(input_data));

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());

  base::test::TestFuture<mojom::ReadBufferResultPtr> future;
  webnn_buffer_remote->ReadBuffer(future.GetCallback());
  mojom::ReadBufferResultPtr result = future.Take();
  ASSERT_FALSE(result->is_error());
  EXPECT_TRUE(IsBufferDataEqual(mojo_base::BigBuffer(input_data),
                                std::move(result->get_buffer())));
}

// Test writing to a WebNNBuffer smaller than the data being written fails.
TEST_F(WebNNBufferImplBackendTest, WriteBufferImplTooLargeTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  if (!CreateWebNNContext(webnn_context_remote)) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  }

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(4ull), base::UnguessableToken::Create());

  webnn_buffer_remote->WriteBuffer(mojo_base::BigBuffer(
      std::array<const uint8_t, 5>({0xBB, 0xBB, 0xBB, 0xBB, 0xBB})));

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

}  // namespace

}  // namespace webnn::test
