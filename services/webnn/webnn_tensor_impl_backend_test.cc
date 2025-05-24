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
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/tensor_impl_dml.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
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
  raw_ptr<WebNNContextProviderImpl, DanglingUntriaged> provider_impl_ = nullptr;
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
  // Testing WebGpuInterop usage relies on the WebNNContextProvider
  // interface implementation in order to lookup WebNNTensorImpls from
  // non-WebNNContextProviderImpls.
  provider_impl_ = WebNNContextProviderImpl::CreateForTesting(
                       webnn_provider_remote_.BindNewPipeAndPassReceiver())
                       .as_ptr();
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
          mojom::Device::kGpu,
          mojom::CreateContextOptions::PowerPreference::kDefault),
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
                                     mojo_base::BigBuffer(0),
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

  EXPECT_TRUE(CreateWebNNTensor(webnn_context_remote,
                                mojom::TensorInfo::New(
                                    OperandDescriptor::UnsafeCreateForTesting(
                                        OperandDataType::kFloat32,
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
      OperandDescriptor::UnsafeCreateForTesting(OperandDataType::kInt32,
                                                std::array<uint32_t, 2>{4, 3}),
      MLTensorUsage());

  EXPECT_TRUE(CreateWebNNTensor(webnn_context_remote, tensor_info->Clone())
                  .has_value());

  EXPECT_TRUE(CreateWebNNTensor(webnn_context_remote, tensor_info->Clone())
                  .has_value());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Test creating a WebNNTensor larger than tensor byte length limit.
// The test is failing on android x86 builds: https://crbug.com/390358145.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
#define MAYBE_CreateTooLargeTensorTest DISABLED_CreateTooLargeTensorTest
#else
#define MAYBE_CreateTooLargeTensorTest CreateTooLargeTensorTest
#endif  // #if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
TEST_F(WebNNTensorImplBackendTest, MAYBE_CreateTooLargeTensorTest) {
  const std::array<uint32_t, 3> large_shape{std::numeric_limits<int32_t>::max(),
                                            2, 2};

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

  // The callback will not be called when the tensor is invalid.
  mojom::WebNNContext::CreateTensorCallback create_tensor_callback =
      base::BindOnce([](mojom::CreateTensorResultPtr create_tensor_result) {});
  webnn_context_remote->CreateTensor(
      mojom::TensorInfo::New(OperandDescriptor::UnsafeCreateForTesting(
                                 OperandDataType::kUint8, large_shape),
                             MLTensorUsage{MLTensorUsageFlags::kWrite}),
      mojo_base::BigBuffer(0), std::move(create_tensor_callback));

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidTensor);
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
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
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
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
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

// Testing for WebGPUInterop requires backend-specific APIs to
// synchronize contents and simulate usage from another command queue.
#if BUILDFLAG(IS_WIN)

class WebNNTensorImplDmlBackendTest : public WebNNTensorImplBackendTest {
 public:
  void SetUp() override {
    WebNNTensorImplBackendTest::SetUp();

    if (!webnn_provider_remote_.is_bound()) {
      GTEST_SKIP() << "WebNN not supported on this platform.";
    }

    base::expected<CreateContextSuccess, webnn::mojom::Error::Code>
        context_result = CreateWebNNContext();
    if (!context_result.has_value() &&
        context_result.error() == mojom::Error::Code::kNotSupportedError) {
      GTEST_SKIP() << "WebNN not supported on this platform.";
    } else {
      webnn_context_remote_ =
          std::move(context_result.value().webnn_context_remote);
      webnn_context_handle_ =
          std::move(context_result.value().webnn_context_handle);
    }

    ASSERT_TRUE(webnn_context_remote_.is_bound());
  }

  base::WeakPtr<native::d3d12::WebNNTensor> GetWebNNTensor(
      const blink::WebNNTensorToken& webnn_tensor_handle) const {
    base::optional_ref<WebNNContextImpl> context_impl =
        provider_impl_->GetWebNNContextImplForTesting(webnn_context_handle_);
    return static_cast<dml::TensorImplDml*>(
               context_impl->GetWebNNTensorImpl(webnn_tensor_handle).as_ptr())
        ->AsWeakPtr();
  }

 protected:
  mojo::Remote<mojom::WebNNContext> webnn_context_remote_;
  blink::WebNNContextToken webnn_context_handle_;
};

void WriteTensorData(base::span<const uint8_t> src_data,
                     ID3D12Resource* dst_buffer) {
  void* mapped_upload_data = nullptr;
  ASSERT_HRESULT_SUCCEEDED(dst_buffer->Map(0, nullptr, &mapped_upload_data));
  // SAFETY: `dst_buffer` was constructed with size `src_data.size()`.
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(mapped_upload_data), src_data.size()))
      .copy_from(src_data);
  dst_buffer->Unmap(0, nullptr);
}

bool IsFenceCompleted(ID3D12Fence* fence, uint64_t fence_value) {
  return fence->GetCompletedValue() >= fence_value;
}

// Verify calling end access twice outputs the same fence and resource.
TEST_F(WebNNTensorImplDmlBackendTest, EndAccessWebNNTwiceTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code>
      create_tensor_result = CreateWebNNTensor(
          webnn_context_remote_,
          mojom::TensorInfo::New(
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWebGpuInterop}));
  if (create_tensor_result.has_value()) {
    webnn_tensor_remote =
        std::move(create_tensor_result.value().webnn_tensor_remote);
    webnn_tensor_handle =
        std::move(create_tensor_result.value().webnn_tensor_handle);
  }

  ASSERT_TRUE(webnn_tensor_remote.is_bound());

  webnn_context_remote_.FlushForTesting();

  base::WeakPtr<native::d3d12::WebNNTensor> webnn_tensor =
      GetWebNNTensor(webnn_tensor_handle);
  ASSERT_TRUE(webnn_tensor);

  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_1 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_1);

  // Ensure nothing to wait for if no WebNN work prior to EndAccessWebNN().
  EXPECT_TRUE(IsFenceCompleted(webnn_fence_to_wait_for_1->GetD3D12Fence().Get(),
                               webnn_fence_to_wait_for_1->GetFenceValue()));

  EXPECT_TRUE(webnn_tensor->BeginAccessWebNN(
      webnn_fence_to_wait_for_1->GetD3D12Fence(),
      webnn_fence_to_wait_for_1->GetFenceValue()));

  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_2 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_2);

  // End access again on the same tensor should return the same fence.
  EXPECT_EQ(webnn_fence_to_wait_for_2->GetD3D12Fence().Get(),
            webnn_fence_to_wait_for_1->GetD3D12Fence().Get());

  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Verify tensor cannot be used before end access.
TEST_F(WebNNTensorImplDmlBackendTest, UsageAfterBeginAccessWebNNTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code>
      create_tensor_result = CreateWebNNTensor(
          webnn_context_remote_,
          mojom::TensorInfo::New(
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWebGpuInterop,
                            MLTensorUsageFlags::kWrite,
                            MLTensorUsageFlags::kRead}));
  if (create_tensor_result.has_value()) {
    webnn_tensor_remote =
        std::move(create_tensor_result.value().webnn_tensor_remote);
    webnn_tensor_handle =
        std::move(create_tensor_result.value().webnn_tensor_handle);
  }

  ASSERT_TRUE(webnn_tensor_remote.is_bound());

  webnn_context_remote_.FlushForTesting();

  base::WeakPtr<native::d3d12::WebNNTensor> webnn_tensor =
      GetWebNNTensor(webnn_tensor_handle);
  ASSERT_TRUE(webnn_tensor);

  // Ensure WebNN can use the tensor before access begins.
  constexpr uint64_t kTensorSize = 4ull;
  const std::array<const uint8_t, kTensorSize> input_data{0xAA, 0xAA, 0xAA,
                                                          0xAA};
  webnn_tensor_remote->WriteTensor(mojo_base::BigBuffer(input_data));
  webnn_tensor_remote.FlushForTesting();

  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for);

  EXPECT_TRUE(
      webnn_tensor->BeginAccessWebNN(webnn_fence_to_wait_for->GetD3D12Fence(),
                                     webnn_fence_to_wait_for->GetFenceValue()));

  // Ensure the WebNN can still use the tensor after begin access.
  {
    base::test::TestFuture<mojom::ReadTensorResultPtr> read_tensor_future;
    webnn_tensor_remote->ReadTensor(read_tensor_future.GetCallback());
    mojom::ReadTensorResultPtr read_create_tensor_result =
        read_tensor_future.Take();
    ASSERT_FALSE(read_create_tensor_result->is_error());
    EXPECT_TRUE(
        IsBufferDataEqual(mojo_base::BigBuffer(input_data),
                          std::move(read_create_tensor_result->get_buffer())));
  }

  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Verify access between queues: WebNN and an external one.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM_FAMILY)
// Test is flaky on Win+arm, see https://crbug.com/416712077.
#define MAYBE_AccessOnDifferentQueueTest DISABLED_AccessOnDifferentQueueTest
#else
#define MAYBE_AccessOnDifferentQueueTest AccessOnDifferentQueueTest
#endif
TEST_F(WebNNTensorImplDmlBackendTest, MAYBE_AccessOnDifferentQueueTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code>
      create_tensor_result = CreateWebNNTensor(
          webnn_context_remote_,
          mojom::TensorInfo::New(
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWebGpuInterop,
                            MLTensorUsageFlags::kRead}));
  if (create_tensor_result.has_value()) {
    webnn_tensor_remote =
        std::move(create_tensor_result.value().webnn_tensor_remote);
    webnn_tensor_handle =
        std::move(create_tensor_result.value().webnn_tensor_handle);
  }

  ASSERT_TRUE(webnn_tensor_remote.is_bound());

  webnn_context_remote_.FlushForTesting();

  // Simulate access by creating an external queue, recorder, and a
  // buffer.
  scoped_refptr<dml::CommandQueue> command_queue =
      dml::CommandQueue::Create(adapter_->d3d12_device());
  ASSERT_NE(command_queue, nullptr);

  auto create_recorder_result =
      dml::CommandRecorder::Create(command_queue, adapter_->dml_device());
  ASSERT_TRUE(create_recorder_result.has_value());
  std::unique_ptr<dml::CommandRecorder> command_recorder =
      std::move(create_recorder_result.value());

  constexpr uint64_t kTensorSize = 4ull;
  const std::array<const uint8_t, kTensorSize> input_data = {0xAA, 0xAA, 0xAA,
                                                             0xAA};
  Microsoft::WRL::ComPtr<ID3D12Resource> upload_buffer;
  ASSERT_HRESULT_SUCCEEDED(
      dml::CreateUploadBuffer(adapter_->d3d12_device(), input_data.size(),
                              L"Upload_Buffer", upload_buffer));
  ASSERT_NE(upload_buffer, nullptr);

  // SAFETY: `upload_buffer` was constructed with size `input_data.size()`.
  UNSAFE_BUFFERS(WriteTensorData(
      base::span(input_data.data(), input_data.size()), upload_buffer.Get()));

  base::WeakPtr<native::d3d12::WebNNTensor> webnn_tensor =
      GetWebNNTensor(webnn_tensor_handle);
  ASSERT_TRUE(webnn_tensor);

  // Simulate multi-queue usage via GPU copy.
  //
  // Step | WebNN queue   |  Other queue
  // -----------------------------------
  // 1.     Signal
  // 2.          |---------> Wait
  // 3.                      GPU copy
  // 4.                      Signal
  // 5.     Wait <-----------|
  // 6.     GPU copy
  // 7.     Signal
  // 8.          |---------> Wait
  // 9.                      GPU copy
  // 10.                     Signal
  // 11.    Wait <-----------|
  // 12.    GPU copy
  // 13.    Signal
  // 14.         |----------> Wait

  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_1 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_1);

  // Step 1. End access with no WebNN work should not require a wait.
  ASSERT_TRUE(IsFenceCompleted(webnn_fence_to_wait_for_1->GetD3D12Fence().Get(),
                               webnn_fence_to_wait_for_1->GetFenceValue()));

  {
    ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());
    UploadBufferWithBarrier(command_recorder.get(),
                            webnn_tensor->GetD3D12Buffer(), upload_buffer,
                            kTensorSize);
    ASSERT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  }

  ASSERT_TRUE(webnn_tensor->BeginAccessWebNN(
      command_queue->submission_fence(), command_queue->GetLastFenceValue()));

  // Step 5. Ensure WebNN can use the tensor after begin access.
  {
    base::test::TestFuture<mojom::ReadTensorResultPtr> read_tensor_future;
    webnn_tensor_remote->ReadTensor(read_tensor_future.GetCallback());
    mojom::ReadTensorResultPtr read_create_tensor_result =
        read_tensor_future.Take();
    ASSERT_FALSE(read_create_tensor_result->is_error());
    EXPECT_TRUE(
        IsBufferDataEqual(mojo_base::BigBuffer(input_data),
                          std::move(read_create_tensor_result->get_buffer())));
  }

  // Step 8. Simulate more external queue use with new data.
  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_2 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_2);

  const std::array<const uint8_t, kTensorSize> new_input_data = {0xBB, 0xBB,
                                                                 0xBB, 0xBB};
  {
    // SAFETY: `upload_buffer` was constructed with size
    // `new_input_data.size()`.
    UNSAFE_BUFFERS(WriteTensorData(
        base::span(new_input_data.data(), new_input_data.size()),
        upload_buffer.Get()));

    ASSERT_HRESULT_SUCCEEDED(command_queue->WaitForFence(
        webnn_fence_to_wait_for_2->GetD3D12Fence(),
        webnn_fence_to_wait_for_2->GetFenceValue()));
    ASSERT_HRESULT_SUCCEEDED(command_recorder->Open());
    UploadBufferWithBarrier(command_recorder.get(),
                            webnn_tensor->GetD3D12Buffer(), upload_buffer,
                            kTensorSize);
    ASSERT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  }

  ASSERT_TRUE(webnn_tensor->BeginAccessWebNN(
      command_queue->submission_fence(), command_queue->GetLastFenceValue()));

  // Step 11. WebNN should be able to use the tensor after begin access.
  {
    base::test::TestFuture<mojom::ReadTensorResultPtr> read_tensor_future;
    webnn_tensor_remote->ReadTensor(read_tensor_future.GetCallback());
    mojom::ReadTensorResultPtr read_create_tensor_result =
        read_tensor_future.Take();
    ASSERT_FALSE(read_create_tensor_result->is_error());
    EXPECT_TRUE(
        IsBufferDataEqual(mojo_base::BigBuffer(new_input_data),
                          std::move(read_create_tensor_result->get_buffer())));
  }

  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Verify end access with no WebNN work in-between returns the last fence
// without WebNN calling wait.
TEST_F(WebNNTensorImplDmlBackendTest, NoWebNNQueueAccessInBetweenTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
  base::expected<CreateTensorSuccess, webnn::mojom::Error::Code>
      create_tensor_result = CreateWebNNTensor(
          webnn_context_remote_,
          mojom::TensorInfo::New(
              OperandDescriptor::UnsafeCreateForTesting(
                  OperandDataType::kUint8, std::array<uint32_t, 2>{2, 2}),
              MLTensorUsage{MLTensorUsageFlags::kWebGpuInterop}));
  if (create_tensor_result.has_value()) {
    webnn_tensor_remote =
        std::move(create_tensor_result.value().webnn_tensor_remote);
    webnn_tensor_handle =
        std::move(create_tensor_result.value().webnn_tensor_handle);
  }

  ASSERT_TRUE(webnn_tensor_remote.is_bound());

  webnn_context_remote_.FlushForTesting();

  // Simulate access by creating an external queue.
  scoped_refptr<dml::CommandQueue> command_queue =
      dml::CommandQueue::Create(adapter_->d3d12_device());
  ASSERT_NE(command_queue, nullptr);

  base::WeakPtr<native::d3d12::WebNNTensor> webnn_tensor =
      GetWebNNTensor(webnn_tensor_handle);
  ASSERT_TRUE(webnn_tensor);

  // End access without any WebNN work prior returns WebNN's submission
  // fence which should be completed.
  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_1 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_1);

  ASSERT_TRUE(IsFenceCompleted(webnn_fence_to_wait_for_1->GetD3D12Fence().Get(),
                               webnn_fence_to_wait_for_1->GetFenceValue()));

  // Initialize the external queue's submission fence to a non-zero value to
  // ensure it has not been signaled by WebNN's queue.
  const uint64_t initialValue = 0xFF;
  command_queue->submission_fence()->Signal(initialValue);

  ASSERT_TRUE(webnn_tensor->BeginAccessWebNN(command_queue->submission_fence(),
                                             initialValue + 1));

  // Calling end access again, with no WebNN work, should
  // return the last fence without WebNN calling wait on it.
  std::unique_ptr<native::d3d12::WebNNSharedFence> webnn_fence_to_wait_for_2 =
      webnn_tensor->EndAccessWebNN();
  ASSERT_TRUE(webnn_fence_to_wait_for_2);

  EXPECT_EQ(command_queue->submission_fence(),
            webnn_fence_to_wait_for_2->GetD3D12Fence().Get());

  EXPECT_FALSE(
      IsFenceCompleted(command_queue->submission_fence(), initialValue + 1));

  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace webnn::test
