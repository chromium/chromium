// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

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

class WebNNContextDMLImplTest : public TestBase {
 protected:
  WebNNContextDMLImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNContextDMLImplTest() override = default;

  bool CreateWebNNContext(
      mojo::Remote<mojom::WebNNContextProvider>& webnn_provider_remote,
      mojo::Remote<mojom::WebNNContext>& webnn_context_remote) {
    bool is_platform_supported = true;

    // Create the dml::ContextImpl through context provider.
    base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
    webnn_provider_remote->CreateWebNNContext(
        mojom::CreateContextOptions::New(),
        create_context_future.GetCallback());
    auto create_context_result = create_context_future.Take();
    if (create_context_result->is_context_remote()) {
      webnn_context_remote.Bind(
          std::move(create_context_result->get_context_remote()));
    } else {
      is_platform_supported = create_context_result->get_error()->code !=
                              mojom::Error::Code::kNotSupportedError;
    }
    return is_platform_supported;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  ASSERT_TRUE(webnn_context_remote.is_bound());

  // Build a simple graph with relu operator.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(input_operand_id, output_operand_id);

  // The dml::GraphImpl should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  webnn_context_remote->CreateGraph(builder.CloneGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());

  // Reset the remote to ensure `WebNNGraphImpl` is released.
  if (create_graph_result->is_graph_remote()) {
    create_graph_result->get_graph_remote().reset();
  }

  // Ensure `WebNNContextImpl::OnConnectionError()` is called and
  // `WebNNContextImpl` is released.
  webnn_context_remote.reset();
  webnn_provider_remote.reset();

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebNNContextDMLImplTest, CreateBufferImplTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  ASSERT_TRUE(webnn_context_remote.is_bound());

  constexpr uint64_t kBufferSize = 4ull;

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Test creating an over-sized WebNNBuffer should always fail.
TEST_F(WebNNContextDMLImplTest, CreateBufferImplOversizedTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  ASSERT_TRUE(webnn_context_remote.is_bound());

  constexpr uint64_t kBufferSizeTooLarge = std::numeric_limits<uint64_t>::max();

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSizeTooLarge),
      base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Creating two or more WebNNBuffer(s) with separate tokens should always
// succeed.
TEST_F(WebNNContextDMLImplTest, CreateBufferImplManyTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  constexpr uint64_t kBufferSize = 4ull;

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_1.is_bound());

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), base::UnguessableToken::Create());

  EXPECT_TRUE(webnn_buffer_remote_2.is_bound());

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());
}

// Creating two or more WebNNBuffer(s) with the same token should always fail.
TEST_F(WebNNContextDMLImplTest, CreateBufferImplManySameTokenTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  constexpr uint64_t kBufferSize = 4ull;

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// Disconnecting a WebNNBuffer should allow another buffer to be created with
// the same token.
TEST_F(WebNNContextDMLImplTest,
       CreateBufferImplManyReuseTokenAfterDisconnectTest) {
  BadMessageTestHelper bad_message_helper;

  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  constexpr uint64_t kBufferSize = 4ull;

  const base::UnguessableToken& buffer_handle =
      base::UnguessableToken::Create();

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_1;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_1.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_buffer_remote_1.reset();

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_2;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_2.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_FALSE(bad_message_helper.GetLastBadMessage().has_value());

  mojo::Remote<mojom::WebNNBuffer> webnn_buffer_remote_3;
  webnn_context_remote->CreateBuffer(
      webnn_buffer_remote_3.BindNewPipeAndPassReceiver(),
      mojom::BufferInfo::New(kBufferSize), buffer_handle);

  webnn_context_remote.FlushForTesting();
  EXPECT_EQ(bad_message_helper.GetLastBadMessage(), kBadMessageInvalidBuffer);
}

// TODO(crbug.com/1472888): Test the buffer gets destroyed.

}  // namespace webnn::dml
