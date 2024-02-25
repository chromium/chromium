// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include <memory>

#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace gpu {
namespace webgpu {

class WebGPUDecoderTest : public ::testing::Test {
 public:
  WebGPUDecoderTest() {}

  void SetUp() override {
    decoder_client_ = std::make_unique<FakeDecoderClient>();
    command_buffer_service_ = std::make_unique<FakeCommandBufferServiceBase>();

    decoder_.reset(WebGPUDecoder::Create(
        decoder_client_.get(), command_buffer_service_.get(), nullptr, nullptr,
        &outputter_, {}, nullptr, DawnCacheOptions(),
        &mock_isolation_key_provider_));
    ASSERT_EQ(decoder_->Initialize(GpuFeatureInfo()), ContextResult::kSuccess);
  }

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

 protected:
  // Mocks for testing.
  StrictMock<MockIsolationKeyProvider> mock_isolation_key_provider_;

  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  std::unique_ptr<WebGPUDecoder> decoder_;
  std::unique_ptr<FakeDecoderClient> decoder_client_;
  gles2::TraceOutputter outputter_;
};

TEST_F(WebGPUDecoderTest, DawnCommands) {
  cmds::DawnCommands cmd;
  cmd.Init(0, 0, 0, 0, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(WebGPUDecoderTest, IsolationKeyFromDocument) {
  blink::DocumentToken document_token;
  blink::WebGPUExecutionContextToken wgpu_context_token(document_token);
  uint64_t high = document_token->GetHighForSerialization();
  uint64_t low = document_token->GetLowForSerialization();
  EXPECT_CALL(mock_isolation_key_provider_,
              GetIsolationKey(wgpu_context_token, _))
      .Times(1);

  cmds::SetWebGPUExecutionContextToken cmd;
  cmd.Init(base::to_underlying(wgpu_context_token.variant_index()), high >> 32,
           high, low >> 32, low);
  ExecuteCmd(cmd);
}

TEST_F(WebGPUDecoderTest, IsolationKeyFromWorker) {
  blink::DedicatedWorkerToken worker_token;
  blink::WebGPUExecutionContextToken wgpu_context_token(worker_token);
  uint64_t high = worker_token->GetHighForSerialization();
  uint64_t low = worker_token->GetLowForSerialization();
  EXPECT_CALL(mock_isolation_key_provider_,
              GetIsolationKey(wgpu_context_token, _))
      .Times(1);

  cmds::SetWebGPUExecutionContextToken cmd;
  cmd.Init(base::to_underlying(wgpu_context_token.variant_index()), high >> 32,
           high, low >> 32, low);
  ExecuteCmd(cmd);
}

}  // namespace webgpu
}  // namespace gpu
