// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_entry.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/image_decode_accelerator_stub.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_stub.h"
#include "url/gurl.h"

using testing::InSequence;
using testing::StrictMock;

namespace gpu {
class MemoryTracker;

namespace {

struct ExpectedCacheEntry {
  uint32_t id = 0u;
  SkISize dimensions;
};

std::unique_ptr<MemoryTracker> CreateMockMemoryTracker(
    const GPUCreateCommandBufferConfig& init_params) {
  return std::make_unique<gles2::MockMemoryTracker>();
}

scoped_refptr<Buffer> MakeBufferForTesting() {
  return MakeMemoryBuffer(sizeof(base::subtle::Atomic32));
}

// This ImageFactory is defined so that we don't have to generate a real
// GpuMemoryBuffer with decoded data in these tests.
class TestImageFactory : public ImageFactory {
 public:
  TestImageFactory() = default;
  ~TestImageFactory() override = default;

  // ImageFactory implementation.
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      SurfaceHandle surface_handle) override {
    return base::MakeRefCounted<gl::GLImageStub>();
  }
  bool SupportsCreateAnonymousImage() const override { return false; }
  scoped_refptr<gl::GLImage> CreateAnonymousImage(const gfx::Size& size,
                                                  gfx::BufferFormat format,
                                                  gfx::BufferUsage usage,
                                                  bool* is_cleared) override {
    NOTREACHED();
    return nullptr;
  }
  unsigned RequiredTextureType() override { return GL_TEXTURE_EXTERNAL_OES; }
  bool SupportsFormatRGB() override { return false; }
};

}  // namespace

// This mock allows individual tests to decide asynchronously when to finish a
// decode by using the FinishOneDecode() method.
class MockImageDecodeAcceleratorWorker : public ImageDecodeAcceleratorWorker {
 public:
  MockImageDecodeAcceleratorWorker(gfx::BufferFormat format_for_decodes)
      : format_for_decodes_(format_for_decodes) {}

  void Decode(std::vector<uint8_t> encoded_data,
              const gfx::Size& output_size,
              CompletedDecodeCB decode_cb) override {
    pending_decodes_.push(PendingDecode{output_size, std::move(decode_cb)});
    DoDecode(output_size);
  }

  void FinishOneDecode(bool success) {
    if (pending_decodes_.empty())
      return;
    PendingDecode next_decode = std::move(pending_decodes_.front());
    pending_decodes_.pop();
    if (success) {
      // We give out a dummy GpuMemoryBufferHandle as the result: since we mock
      // the ImageFactory and the gl::GLImage in these tests, the only
      // requirement is that the NativePixmapHandle has the right number of
      // planes.
      auto decode_result = std::make_unique<DecodeResult>();
      decode_result->handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
      for (size_t plane = 0; plane < gfx::NumberOfPlanesForLinearBufferFormat(
                                         format_for_decodes_);
           plane++) {
        decode_result->handle.native_pixmap_handle.planes.emplace_back(
            0 /* stride */, 0 /* offset */, 0 /* size */, base::ScopedFD());
      }
      decode_result->visible_size = next_decode.output_size;
      decode_result->buffer_format = format_for_decodes_;
      decode_result->buffer_byte_size = 0u;
      std::move(next_decode.decode_cb).Run(std::move(decode_result));
    } else {
      std::move(next_decode.decode_cb).Run(nullptr);
    }
  }

  MOCK_METHOD1(DoDecode, void(const gfx::Size&));
  MOCK_METHOD0(GetSupportedProfiles,
               std::vector<ImageDecodeAcceleratorSupportedProfile>());

 private:
  struct PendingDecode {
    gfx::Size output_size;
    CompletedDecodeCB decode_cb;
  };

  const gfx::BufferFormat format_for_decodes_;
  base::queue<PendingDecode> pending_decodes_;

  DISALLOW_COPY_AND_ASSIGN(MockImageDecodeAcceleratorWorker);
};

const int kChannelId = 1;

const int32_t kCommandBufferRouteId =
    static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;

// Test fixture: the general strategy for testing is to have a GPU channel test
// infrastructure (provided by GpuChannelTestCommon), ask the channel to handle
// decode requests, and expect sync token releases, invocations to the
// ImageDecodeAcceleratorWorker functionality, and transfer cache entry
// creation.
class ImageDecodeAcceleratorStubTest
    : public GpuChannelTestCommon,
      public ::testing::WithParamInterface<gfx::BufferFormat> {
 public:
  ImageDecodeAcceleratorStubTest()
      : GpuChannelTestCommon(false /* use_stub_bindings */),
        image_decode_accelerator_worker_(GetParam()) {}
  ~ImageDecodeAcceleratorStubTest() override = default;

  SyncPointManager* sync_point_manager() const {
    return channel_manager()->sync_point_manager();
  }

  ServiceTransferCache* GetServiceTransferCache() {
    ContextResult context_result;
    scoped_refptr<SharedContextState> shared_context_state =
        channel_manager()->GetSharedContextState(&context_result);
    if (context_result != ContextResult::kSuccess || !shared_context_state) {
      return nullptr;
    }
    return shared_context_state->transfer_cache();
  }

  int GetRasterDecoderId() {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    DCHECK(channel);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    if (!command_buffer || !command_buffer->decoder_context())
      return -1;
    return command_buffer->decoder_context()->GetRasterDecoderId();
  }

  void SetUp() override {
    GpuChannelTestCommon::SetUp();

    // TODO(andrescj): get rid of the |feature_list_| when the feature is
    // enabled by default.
    feature_list_.InitAndEnableFeature(
        features::kVaapiJpegImageDecodeAcceleration);
    channel_manager()->SetImageDecodeAcceleratorWorkerForTesting(
        &image_decode_accelerator_worker_);

    // Initialize the GrContext so that texture uploading works.
    ContextResult context_result;
    scoped_refptr<SharedContextState> shared_context_state =
        channel_manager()->GetSharedContextState(&context_result);
    ASSERT_EQ(ContextResult::kSuccess, context_result);
    ASSERT_TRUE(shared_context_state);
    shared_context_state->InitializeGrContext(GpuDriverBugWorkarounds(),
                                              nullptr);

    GpuChannel* channel = CreateChannel(kChannelId, false /* is_gpu_host */);
    ASSERT_TRUE(channel);
    ASSERT_TRUE(channel->GetImageDecodeAcceleratorStub());
    channel->GetImageDecodeAcceleratorStub()->SetImageFactoryForTesting(
        &image_factory_);

    // Create a raster command buffer so that the ImageDecodeAcceleratorStub can
    // have access to a TransferBufferManager. Note that we mock the
    // MemoryTracker because GpuCommandBufferMemoryTracker uses a timer that
    // would make RunTasksUntilIdle() run forever.
    CommandBufferStub::SetMemoryTrackerFactoryForTesting(
        base::BindRepeating(&CreateMockMemoryTracker));
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = kNullSurfaceHandle;
    init_params.share_group_id = MSG_ROUTING_NONE;
    init_params.stream_id = 0;
    init_params.stream_priority = SchedulingPriority::kNormal;
    init_params.attribs = ContextCreationAttribs();
    init_params.attribs.enable_gles2_interface = false;
    init_params.attribs.enable_raster_interface = true;
    init_params.attribs.bind_generates_resource = false;
    init_params.active_url = GURL();
    ContextResult result = ContextResult::kTransientFailure;
    Capabilities capabilities;
    HandleMessage(channel,
                  new GpuChannelMsg_CreateCommandBuffer(
                      init_params, kCommandBufferRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities));
    ASSERT_EQ(ContextResult::kSuccess, result);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    ASSERT_TRUE(command_buffer);

    // Make sure there are no pending tasks before starting the test.
    ASSERT_EQ(0u, task_runner()->NumPendingTasks());
    ASSERT_EQ(0u, io_task_runner()->NumPendingTasks());
  }

  void TearDown() override {
    // Make sure the channel is destroyed before the
    // |image_decode_accelerator_worker_| is destroyed.
    channel_manager()->DestroyAllChannels();
  }

  // Intended to run as a task in the GPU scheduler (in the raster sequence):
  // registers |buffer| in the TransferBufferManager and releases the sync token
  // corresponding to |handle_release_count|.
  void RegisterDiscardableHandleBuffer(int32_t shm_id,
                                       scoped_refptr<Buffer> buffer,
                                       uint64_t handle_release_count) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    DCHECK(channel);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    CHECK(command_buffer);
    command_buffer->RegisterTransferBufferForTest(shm_id, std::move(buffer));
    command_buffer->OnFenceSyncRelease(handle_release_count);
  }

  // Creates a discardable handle and schedules a task in the GPU scheduler (in
  // the raster sequence) to register the handle's buffer and release the sync
  // token corresponding to |handle_release_count| (see the
  // RegisterDiscardableHandleBuffer() method). Returns an invalid handle if the
  // command buffer doesn't exist.
  ClientDiscardableHandle CreateDiscardableHandle(
      uint64_t handle_release_count) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    DCHECK(channel);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    if (!command_buffer)
      return ClientDiscardableHandle();
    ClientDiscardableHandle handle(MakeBufferForTesting() /* buffer */,
                                   0u /* byte_offset */,
                                   GetNextBufferId() /* shm_id */);
    scheduler()->ScheduleTask(Scheduler::Task(
        command_buffer->sequence_id(),
        base::BindOnce(
            &ImageDecodeAcceleratorStubTest::RegisterDiscardableHandleBuffer,
            weak_ptr_factory_.GetWeakPtr(), handle.shm_id(),
            handle.BufferForTesting(), handle_release_count) /* closure */,
        std::vector<SyncToken>() /* sync_token_fences */));
    return handle;
  }

  // Sends a decode request IPC and returns a sync token that is expected to be
  // released upon the completion of the decode. The caller is responsible for
  // keeping track of the release count for the decode sync token
  // (|decode_release_count|), the transfer cache entry ID
  // (|transfer_cache_entry_id|), and the release count of the sync token that
  // is signaled after the discardable handle's buffer has been registered in
  // the TransferBufferManager. If the discardable handle can't be created, this
  // function returns an empty sync token.
  SyncToken SendDecodeRequest(const gfx::Size& output_size,
                              uint64_t decode_release_count,
                              uint32_t transfer_cache_entry_id,
                              uint64_t handle_release_count) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    DCHECK(channel);

    // Create the decode sync token for the decode request so that we can test
    // that it's actually released.
    SyncToken decode_sync_token(
        CommandBufferNamespace::GPU_IO,
        CommandBufferIdFromChannelAndRoute(
            kChannelId, static_cast<int32_t>(
                            GpuChannelReservedRoutes::kImageDecodeAccelerator)),
        decode_release_count);

    // Create a discardable handle and schedule its buffer's registration.
    ClientDiscardableHandle handle =
        CreateDiscardableHandle(handle_release_count);
    if (!handle.IsValid())
      return SyncToken();

    // Send the IPC decode request.
    GpuChannelMsg_ScheduleImageDecode_Params decode_params;
    decode_params.encoded_data = std::vector<uint8_t>();
    decode_params.output_size = output_size;
    decode_params.raster_decoder_route_id = kCommandBufferRouteId;
    decode_params.transfer_cache_entry_id = transfer_cache_entry_id;
    decode_params.discardable_handle_shm_id = handle.shm_id();
    decode_params.discardable_handle_shm_offset = handle.byte_offset();
    decode_params.discardable_handle_release_count = handle_release_count;
    decode_params.target_color_space = gfx::ColorSpace();
    decode_params.needs_mips = false;

    HandleMessage(
        channel,
        new GpuChannelMsg_ScheduleImageDecode(
            static_cast<int32_t>(
                GpuChannelReservedRoutes::kImageDecodeAccelerator),
            std::move(decode_params), decode_sync_token.release_count()));
    return decode_sync_token;
  }

  void RunTasksUntilIdle() {
    while (task_runner()->HasPendingTask() ||
           io_task_runner()->HasPendingTask()) {
      task_runner()->RunUntilIdle();
      io_task_runner()->RunUntilIdle();
    }
  }

  void CheckTransferCacheEntries(
      const std::vector<ExpectedCacheEntry>& expected_entries) {
    ServiceTransferCache* transfer_cache = GetServiceTransferCache();
    ASSERT_TRUE(transfer_cache);

    // First, check the number of entries and early out if 0 entries are
    // expected.
    const size_t num_actual_cache_entries =
        transfer_cache->entries_count_for_testing();
    ASSERT_EQ(expected_entries.size(), num_actual_cache_entries);
    if (expected_entries.empty())
      return;

    // Then, check the dimensions of the entries to make sure they are as
    // expected.
    int raster_decoder_id = GetRasterDecoderId();
    ASSERT_GE(raster_decoder_id, 0);
    for (size_t i = 0; i < num_actual_cache_entries; i++) {
      auto* decode_entry = static_cast<cc::ServiceImageTransferCacheEntry*>(
          transfer_cache->GetEntry(ServiceTransferCache::EntryKey(
              raster_decoder_id, cc::TransferCacheEntryType::kImage,
              expected_entries[i].id)));
      ASSERT_TRUE(decode_entry);
      ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetParam()),
                decode_entry->plane_images().size());
      for (size_t plane = 0; plane < decode_entry->plane_images().size();
           plane++) {
        ASSERT_TRUE(decode_entry->plane_images()[plane]);
        EXPECT_TRUE(decode_entry->plane_images()[plane]->isTextureBacked());
      }
      ASSERT_TRUE(decode_entry->image());
      EXPECT_EQ(expected_entries[i].dimensions.width(),
                decode_entry->image()->dimensions().width());
      EXPECT_EQ(expected_entries[i].dimensions.height(),
                decode_entry->image()->dimensions().height());
    }
  }

 protected:
  StrictMock<MockImageDecodeAcceleratorWorker> image_decode_accelerator_worker_;

 private:
  TestImageFactory image_factory_;
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<ImageDecodeAcceleratorStubTest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeAcceleratorStubTest);
};

// Tests the following flow: two decode requests are sent. One of the decodes is
// completed. This should cause one sync token to be released and the scheduler
// sequence to be disabled. Then, the second decode is completed. This should
// cause the other sync token to be released.
TEST_P(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedAfterSequenceIsDisabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode1_sync_token.HasData());
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* decode_release_count */,
      2u /* transfer_cache_entry_id */, 2u /* handle_release_count */);
  ASSERT_TRUE(decode2_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // Only the first decode sync token should be released after the first decode
  // is finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // The second decode sync token should be released after the second decode is
  // finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // Check that the decoded images are in the transfer cache.
  CheckTransferCacheEntries(
      {{1u, SkISize::Make(100, 100)}, {2u, SkISize::Make(200, 200)}});
}

// Tests the following flow: three decode requests are sent. The first decode
// completes which should cause the scheduler sequence to be enabled. Right
// after that (while the sequence is still enabled), the other two decodes
// complete. At the end, all the sync tokens should be released.
TEST_P(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedWhileSequenceIsEnabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode1_sync_token.HasData());
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* decode_release_count */,
      2u /* transfer_cache_entry_id */, 2u /* handle_release_count */);
  ASSERT_TRUE(decode2_sync_token.HasData());
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* decode_release_count */,
      3u /* transfer_cache_entry_id */, 3u /* handle_release_count */);
  ASSERT_TRUE(decode3_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // All decode sync tokens should be released after completing all the decodes.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // Check that the decoded images are in the transfer cache.
  CheckTransferCacheEntries({{1u, SkISize::Make(100, 100)},
                             {2u, SkISize::Make(200, 200)},
                             {3u, SkISize::Make(300, 300)}});
}

// Tests the following flow: three decode requests are sent. The first decode
// fails, the second succeeds, and the third one fails.
TEST_P(ImageDecodeAcceleratorStubTest, FailedDecodes) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode1_sync_token.HasData());
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* decode_release_count */,
      2u /* transfer_cache_entry_id */, 2u /* handle_release_count */);
  ASSERT_TRUE(decode2_sync_token.HasData());
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* decode_release_count */,
      3u /* transfer_cache_entry_id */, 3u /* handle_release_count */);
  ASSERT_TRUE(decode3_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // All decode sync tokens should be released after completing all the decodes.
  image_decode_accelerator_worker_.FinishOneDecode(false);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(false);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // There should only be one image in the transfer cache (the one that
  // succeeded).
  CheckTransferCacheEntries({{2u, SkISize::Make(200, 200)}});
}

TEST_P(ImageDecodeAcceleratorStubTest, OutOfOrderDecodeSyncTokens) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 2u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode1_sync_token.HasData());

  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 1u /* decode_release_count */,
      2u /* transfer_cache_entry_id */, 2u /* handle_release_count */);
  ASSERT_TRUE(decode2_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // Since the sync tokens are out of order, releasing the first one should also
  // release the second one.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // We only expect the first image in the transfer cache.
  CheckTransferCacheEntries({{1u, SkISize::Make(100, 100)}});

  // Finishing the second decode should not "unrelease" the first sync token.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  CheckTransferCacheEntries(
      {{1u, SkISize::Make(100, 100)}, {2u, SkISize::Make(200, 200)}});
}

TEST_P(ImageDecodeAcceleratorStubTest, ZeroReleaseCountDecodeSyncToken) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
      .Times(1);
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 0u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode_sync_token.HasData());

  // A zero-release count sync token is always considered released.
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Even though the release count is not really valid, we can still finish the
  // decode.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
  CheckTransferCacheEntries({{1u, SkISize::Make(100, 100)}});
}

TEST_P(ImageDecodeAcceleratorStubTest, ZeroWidthOutputSize) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(0, 100)))
      .Times(1);
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(0, 100) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Even though the output size is not valid, we can still finish the decode.
  // We just shouldn't get any entries in the transfer cache.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
  CheckTransferCacheEntries({});
}

TEST_P(ImageDecodeAcceleratorStubTest, ZeroHeightOutputSize) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 0)))
      .Times(1);
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 0) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Even though the output size is not valid, we can still finish the decode.
  // We just shouldn't get any entries in the transfer cache.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
  CheckTransferCacheEntries({});
}

// Tests that we wait for a discardable handle's buffer to be registered before
// we attempt to process the corresponding completed decode.
TEST_P(ImageDecodeAcceleratorStubTest, WaitForDiscardableHandleRegistration) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
      .Times(1);

  // First, we disable the raster sequence so that we can control when to
  // register the discardable handle's buffer by re-enabling the sequence.
  GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
  ASSERT_TRUE(channel);
  const CommandBufferStub* command_buffer =
      channel->LookupCommandBuffer(kCommandBufferRouteId);
  ASSERT_TRUE(command_buffer);
  const SequenceId raster_sequence_id = command_buffer->sequence_id();
  scheduler()->DisableSequence(raster_sequence_id);

  // Now we can send the decode request. This schedules the registration of the
  // discardable handle, but it won't actually be registered until we re-enable
  // the raster sequence later on.
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* decode_release_count */,
      1u /* transfer_cache_entry_id */, 1u /* handle_release_count */);
  ASSERT_TRUE(decode_sync_token.HasData());

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Even when a decode is finished, the decode sync token shouldn't be released
  // before the discardable handle's buffer is registered.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Now let's register the discardable handle's buffer by re-enabling the
  // raster sequence. This should trigger the processing of the completed decode
  // and the subsequent release of the decode sync token.
  scheduler()->EnableSequence(raster_sequence_id);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // Check that the decoded images are in the transfer cache.
  CheckTransferCacheEntries({{1u, SkISize::Make(100, 100)}});
}

// TODO(andrescj): test the deletion of transfer cache entries.

INSTANTIATE_TEST_SUITE_P(
    ,
    ImageDecodeAcceleratorStubTest,
    ::testing::Values(gfx::BufferFormat::YVU_420,
                      gfx::BufferFormat::YUV_420_BIPLANAR));

}  // namespace gpu
