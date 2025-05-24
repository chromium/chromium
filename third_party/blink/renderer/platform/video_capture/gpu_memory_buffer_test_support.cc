// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/video_capture/gpu_memory_buffer_test_support.h"

#include "components/viz/test/test_context_provider.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace blink {

TestingPlatformSupportForGpuMemoryBuffer::
    TestingPlatformSupportForGpuMemoryBuffer()
    : sii_(base::MakeRefCounted<gpu::TestSharedImageInterface>()),
      gpu_factories_(new media::MockGpuVideoAcceleratorFactories(sii_.get())),
      media_thread_("TestingMediaThread") {
  // Ensure that any mappable SharedImages created via this testing platform
  // create fake GMBs internally.
  sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
  gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  media_thread_.Start();
  ON_CALL(*gpu_factories_, GetTaskRunner())
      .WillByDefault(Return(media_thread_.task_runner()));
  ON_CALL(*gpu_factories_, ContextCapabilities())
      .WillByDefault(testing::Invoke([&]() { return capabilities_; }));
}

TestingPlatformSupportForGpuMemoryBuffer::
    ~TestingPlatformSupportForGpuMemoryBuffer() {
  media_thread_.Stop();
}

media::GpuVideoAcceleratorFactories*
TestingPlatformSupportForGpuMemoryBuffer::GetGpuFactories() {
  return gpu_factories_.get();
}

void TestingPlatformSupportForGpuMemoryBuffer::SetGpuCapabilities(
    gpu::Capabilities* capabilities) {
  capabilities_ = capabilities;
}

void TestingPlatformSupportForGpuMemoryBuffer::SetSharedImageCapabilities(
    const gpu::SharedImageCapabilities& shared_image_capabilities) {
  sii_->SetCapabilities(shared_image_capabilities);
}

}  // namespace blink
