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
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
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
  blink::WebNNContextToken webnn_context_handle;
};

struct CreateTensorSuccess {
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
};

#if BUILDFLAG(IS_WIN)
class WebNNTensorImplBackendTest : public dml::TestBase {
 public:
  WebNNTensorImplBackendTest()
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

void WebNNTensorImplBackendTest::SetUp() {
  SKIP_TEST_IF(!dml::UseGPUInTests());

  dml::Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = dml::Adapter::GetGpuInstanceForTesting();
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
class WebNNTensorImplBackendTest : public testing::Test {
 public:
  WebNNTensorImplBackendTest()
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

void WebNNTensorImplBackendTest::SetUp() {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }

  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());

  GTEST_SKIP() << "WebNNTensor not implemented on macOS";
}
#elif BUILDFLAG(WEBNN_USE_TFLITE)
class WebNNTensorImplBackendTest : public testing::Test {
 public:
  WebNNTensorImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {
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
};
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

void WebNNTensorImplBackendTest::TearDown() {
  webnn_provider_remote_.reset();
  base::RunLoop().RunUntilIdle();
}

base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
WebNNTensorImplBackendTest::CreateWebNNContext() {
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

base::expected<CreateTensorSuccess, webnn::mojom::Error::Code>
CreateWebNNTensor(mojo::Remote<mojom::WebNNContext>& webnn_context_remote,
                  mojom::TensorInfoPtr tensor_info) {
  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  webnn_context_remote->CreateTensor(std::move(tensor_info),
                                     create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  if (create_tensor_result->is_success()) {
    mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
    webnn_tensor_remote.Bind(
        std::move(create_tensor_result->get_success()->tensor_remote));
    return CreateTensorSuccess{
        std::move(webnn_tensor_remote),
        std::move(create_tensor_result->get_success()->tensor_handle)};
  } else {
    return base::unexpected(create_tensor_result->get_error()->code);
  }
}

bool IsBufferDataEqual(const mojo_base::BigBuffer& a,
                       const mojo_base::BigBuffer& b) {
  return base::span(a) == base::span(b);
}

TEST_F(WebNNTensorImplBackendTest, CreateTensorImplTest) {
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

  EXPECT_TRUE(CreateWebNNTensor(
                  webnn_context_remote,
                  mojom::TensorInfo::New(
                      *OperandDescriptor::Create(OperandDataType::kFloat32,
                                                 std::array<uint32_t, 2>{3, 4}),
                      MLTensorUsage()))
                  .has_value());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Creating two or more WebNNTensor(s) with separate tokens should always
// succeed.
TEST_F(WebNNTensorImplBackendTest, CreateTensorImplManyTest) {
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

  const auto tensor_info = mojom::TensorInfo::New(
      *OperandDescriptor::Create(OperandDataType::kInt32,
                                 std::array<uint32_t, 2>{4, 3}),
      MLTensorUsage());

  EXPECT_TRUE(CreateWebNNTensor(webnn_context_remote, tensor_info->Clone())
                  .has_value());

  EXPECT_TRUE(CreateWebNNTensor(webnn_context_remote, tensor_info->Clone())
                  .has_value());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// TODO(https://crbug.com/40278771): Test the tensor gets destroyed.

TEST_F(WebNNTensorImplBackendTest, WriteTensorImplTest) {
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

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code> tensor_result =
      CreateWebNNTensor(
          webnn_context_remote,
          mojom::TensorInfo::New(
              *OperandDescriptor::Create(OperandDataType::kUint8,
                                         std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWrite,
                            MLTensorUsageFlags::kRead}));
  if (tensor_result.has_value()) {
    webnn_tensor_remote = std::move(tensor_result.value().webnn_tensor_remote);
  }

  EXPECT_TRUE(webnn_tensor_remote.is_bound());

  const std::array<const uint8_t, 4> input_data{0xAA, 0xAA, 0xAA, 0xAA};
  webnn_tensor_remote->WriteTensor(mojo_base::BigBuffer(input_data));

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());

  base::test::TestFuture<mojom::ReadTensorResultPtr> future;
  webnn_tensor_remote->ReadTensor(future.GetCallback());
  mojom::ReadTensorResultPtr result = future.Take();
  ASSERT_FALSE(result->is_error());
  EXPECT_TRUE(IsBufferDataEqual(mojo_base::BigBuffer(input_data),
                                std::move(result->get_buffer())));
}

// Test writing to a WebNNTensor smaller than the data being written fails.
TEST_F(WebNNTensorImplBackendTest, WriteTensorImplTooLargeTest) {
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

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code> tensor_result =
      CreateWebNNTensor(
          webnn_context_remote,
          mojom::TensorInfo::New(
              *OperandDescriptor::Create(OperandDataType::kUint8,
                                         std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWrite}));
  if (tensor_result.has_value()) {
    webnn_tensor_remote = std::move(tensor_result.value().webnn_tensor_remote);
  }

  EXPECT_TRUE(webnn_tensor_remote.is_bound());

  webnn_tensor_remote->WriteTensor(mojo_base::BigBuffer(
      std::array<const uint8_t, 5>({0xBB, 0xBB, 0xBB, 0xBB, 0xBB})));

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidTensor);
}

// Creating two or more WebNNContexts(s) with separate tokens should always
// succeed.
TEST_F(WebNNTensorImplBackendTest, CreateContextImplManyTest) {
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
