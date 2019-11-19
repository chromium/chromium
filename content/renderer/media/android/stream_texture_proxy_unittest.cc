// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "content/renderer/stream_texture_host_android.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// GpuChannelHost is expected to be created on the IO thread, and posts tasks to
// setup its IPC listener, so it must be created after the thread task runner
// handle is set.
class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  TestGpuChannelHost()
      : gpu::GpuChannelHost(
            0 /* channel_id */,
            gpu::GPUInfo(),
            gpu::GpuFeatureInfo(),
            mojo::ScopedMessagePipeHandle(
                mojo::MessagePipeHandle(mojo::kInvalidHandleValue))) {}

 protected:
  ~TestGpuChannelHost() override {}
};

class StreamTextureProxyTest : public testing::Test {
 public:
  StreamTextureProxyTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        thread_task_runner_handle_override_(
            base::ThreadTaskRunnerHandle::OverrideForTesting(task_runner_)),
        channel_(base::MakeRefCounted<TestGpuChannelHost>()) {}

  ~StreamTextureProxyTest() override { channel_ = nullptr; }

  std::unique_ptr<StreamTextureProxy> CreateProxyWithNullGpuChannelHost() {
    // Create the StreamTextureHost with a valid |channel_|. Note that route_id
    // does not matter here for the test we are writing.
    auto host = std::make_unique<StreamTextureHost>(channel_, 1 /* route_id */);

    // Force the StreamTextureHost's |channel_| to be null by calling
    // OnChannelError.
    host->OnChannelError();
    auto proxy = base::WrapUnique(new StreamTextureProxy(std::move(host)));
    return proxy;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedClosureRunner thread_task_runner_handle_override_;
  scoped_refptr<TestGpuChannelHost> channel_;
};

// This test is to make sure CreateSharedImage() do not crash even if
// StreamTextureHost's |channel_| is null.
TEST_F(StreamTextureProxyTest,
       CreateSharedImageDoesNotCrashWithNullGpuChannelHost) {
  auto proxy = CreateProxyWithNullGpuChannelHost();
  gpu::Mailbox mailbox;
  gpu::SyncToken texture_mailbox_sync_token;

  // This method should not crash even if the StreamTextureHost's |channel_| is
  // null.
  proxy->CreateSharedImage(gfx::Size(1, 1), &mailbox,
                           &texture_mailbox_sync_token);
}

}  // namespace content
