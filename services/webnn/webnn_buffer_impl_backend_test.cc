// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/ml_buffer_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
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

struct CreateContextSuccess {
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::UnguessableToken webnn_context_handle;
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
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
  CreateWebNNContext();

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
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
  CreateWebNNContext();

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
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
  CreateWebNNContext();

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

base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
WebNNBufferImplBackendTest::CreateWebNNContext() {
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote_->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kGpu,
          mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      create_context_future.GetCallback());
  auto create_context_result = create_context_future.Take();
  if (create_context_result->is_success()) {
    mojo::Remote<mojom::WebNNContext> webnn_context_remote;
    webnn_context_remote.Bind(
        std::move(create_context_result->get_success()->context_remote));
    return CreateContextSuccess{
        std::move(webnn_context_remote),
        std::move(create_context_result->get_success()->context_handle)};
  } else {
    return base::unexpected(create_context_result->get_error()->code);
  }
}

bool IsBufferDataEqual(const mojo_base::BigBuffer& a,
                       const mojo_base::BigBuffer& b) {
  return base::span(a) == base::span(b);
}

TEST_F(WebNNBufferImplBackendTest, CreateBufferImplTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  ASSERT_TRUE(webnn_context_remote.is_bound());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(
          *OperandDescriptor::Create(OperandDataType::kFloat32,
                                     std::array<uint32_t, 2>{3, 4}),
          MLBufferUsage()),
      base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Creating two or more WebNNBuffer(s) with separate tokens should always
// succeed.
TEST_F(WebNNBufferImplBackendTest, CreateBufferImplManyTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  const auto buffer_info = mojom::BufferInfo::New(
      *OperandDescriptor::Create(OperandDataType::kInt32,
                                 std::array<uint32_t, 2>{4, 3}),
      MLBufferUsage());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_1.is_bound());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_2.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Creating two or more WebNNBuffer(s) with the same token should always fail.
TEST_F(WebNNBufferImplBackendTest, CreateBufferImplManySameTokenTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  const auto buffer_info = mojom::BufferInfo::New(
      *OperandDescriptor::Create(OperandDataType::kFloat16,
                                 std::array<uint32_t, 2>{2, 3}),
      MLBufferUsage());

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), buffer_handle);

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Disconnecting a WebNNBuffer should allow another buffer to be created with
// the same token.
TEST_F(WebNNBufferImplBackendTest,
       CreateBufferImplManyReuseTokenAfterDisconnectTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  const auto buffer_info = mojom::BufferInfo::New(
      *OperandDescriptor::Create(OperandDataType::kFloat32,
                                 std::array<uint32_t, 3>{2, 2, 3}),
      MLBufferUsage());

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), buffer_handle);

  webnn_buffer_remote_1.reset();

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote_3;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_3.BindNewEndpointAndPassReceiver(),
      buffer_info->Clone(), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// TODO(https://crbug.com/40278771): Test the buffer gets destroyed.

TEST_F(WebNNBufferImplBackendTest, WriteBufferImplTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(
          *OperandDescriptor::Create(OperandDataType::kUint8,
                                     std::array<uint32_t, 2>{2, 2}),
          MLBufferUsage()),
      base::UnguessableToken::Create());

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
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_result = CreateWebNNContext();
  if (!context_result.has_value() &&
      context_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote =
        std::move(context_result.value().webnn_context_remote);
  }

  mojo::AssociatedRemote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewEndpointAndPassReceiver(),
      mojom::BufferInfo::New(
          *OperandDescriptor::Create(OperandDataType::kUint8,
                                     std::array<uint32_t, 2>{2, 2}),
          MLBufferUsage()),
      base::UnguessableToken::Create());

  webnn_buffer_remote->WriteBuffer(mojo_base::BigBuffer(
      std::array<const uint8_t, 5>({0xBB, 0xBB, 0xBB, 0xBB, 0xBB})));

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Creating two or more WebNNContexts(s) with separate tokens should always
// succeed.
TEST_F(WebNNBufferImplBackendTest, CreateContextImplManyTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContext> webnn_context_remote_1;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_1_result = CreateWebNNContext();
  if (!context_1_result.has_value() &&
      context_1_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote_1 =
        std::move(context_1_result.value().webnn_context_remote);
  }

  EXPECT_TRUE(webnn_context_remote_1.is_bound());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote_2;
  base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
      context_2_result = CreateWebNNContext();
  if (!context_2_result.has_value() &&
      context_2_result.error() == mojom::Error::Code::kNotSupportedError) {
    GTEST_SKIP() << "WebNN not supported on this platform.";
  } else {
    webnn_context_remote_2 =
        std::move(context_2_result.value().webnn_context_remote);
  }

  EXPECT_TRUE(webnn_context_remote_2.is_bound());

  webnn_provider_remote_.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

}  // namespace

}  // namespace webnn::test
