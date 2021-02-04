// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace webgpu {

class WebGPUDecoderTest : public ::testing::Test {
 public:
  WebGPUDecoderTest() {}

  void SetUp() override {
    if (!WebGPUSupported()) {
      return;
    }
    decoder_client_.reset(new FakeDecoderClient());
    command_buffer_service_.reset(new FakeCommandBufferServiceBase());
    decoder_.reset(WebGPUDecoder::Create(
        decoder_client_.get(), command_buffer_service_.get(), nullptr, nullptr,
        &outputter_, GpuPreferences()));
    ASSERT_EQ(decoder_->Initialize(), ContextResult::kSuccess);

    constexpr uint32_t kAdapterClientID = 0;
    cmds::RequestAdapter requestAdapterCmd;
    requestAdapterCmd.Init(
        kAdapterClientID,
        static_cast<uint32_t>(webgpu::PowerPreference::kHighPerformance));
    ASSERT_EQ(error::kNoError, ExecuteCmd(requestAdapterCmd));

    constexpr uint32_t kAdapterServiceID = 0;
    cmds::RequestDevice requestDeviceCmd;
    requestDeviceCmd.Init(0, kAdapterServiceID, 1, 0, 0, 0, 0);
    ASSERT_EQ(error::kNoError, ExecuteCmd(requestDeviceCmd));
  }

  bool WebGPUSupported() const {
    // WebGPU does not work on Win7 because there is no D3D12 on Win7
    return !GPUTestBotConfig::CurrentConfigMatches("Win7");
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
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  std::unique_ptr<WebGPUDecoder> decoder_;
  std::unique_ptr<FakeDecoderClient> decoder_client_;
  gles2::TraceOutputter outputter_;
};

TEST_F(WebGPUDecoderTest, DawnCommands) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  cmds::DawnCommands cmd;
  cmd.Init(0, 0, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

}  // namespace webgpu
}  // namespace gpu
