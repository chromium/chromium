// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_entry.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "skia/ext/skia_memory_dump_provider.h"
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
#include "url/gurl.h"

using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::StrictMock;

namespace gpu {
class MemoryTracker;

namespace {

// The size of a decoded buffer to report for a successful decode.
constexpr size_t kDecodedBufferByteSize = 123u;

// The byte size Skia is expected to report for a buffer object.
constexpr uint64_t kSkiaBufferObjectSize = 32768;

struct ExpectedCacheEntry {
  uint32_t id = 0u;
  SkISize dimensions;
};

std::unique_ptr<MemoryTracker> CreateMockMemoryTracker() {
  return std::make_unique<NiceMock<gles2::MockMemoryTracker>>();
}

scoped_refptr<Buffer> MakeBufferForTesting() {
  return MakeMemoryBuffer(sizeof(base::subtle::Atomic32));
}

uint64_t GetMemoryDumpByteSize(
    const base::trace_event::MemoryAllocatorDump* dump,
    const std::string& entry_name) {
  DCHECK(dump);
  auto entry_it =
      base::ranges::find(dump->entries(), entry_name,
                         &base::trace_event::MemoryAllocatorDump::Entry::name);
  if (entry_it != dump->entries().cend()) {
    EXPECT_EQ(std::string(base::trace_event::MemoryAllocatorDump::kUnitsBytes),
              entry_it->units);
    EXPECT_EQ(base::trace_event::MemoryAllocatorDump::Entry::EntryType::kUint64,
              entry_it->entry_type);
    return entry_it->value_uint64;
  }
  EXPECT_TRUE(false);
  return 0u;
}

base::CheckedNumeric<uint64_t> GetExpectedTotalMippedSizeForPlanarImage(
    const cc::ServiceImageTransferCacheEntry* decode_entry) {
  base::CheckedNumeric<uint64_t> safe_total_image_size = 0u;
  for (const auto& plane_image : decode_entry->plane_images()) {
    safe_total_image_size +=
        base::strict_cast<uint64_t>(plane_image->textureSize());
  }
  return safe_total_image_size;
}

class TestSharedImageBackingFactory : public SharedImageBackingFactory {
 public:
  TestSharedImageBackingFactory() : SharedImageBackingFactory(kUsageAll) {}

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) override {
    auto test_image_backing = std::make_unique<TestImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        0);

    // If the backing is not cleared, SkiaImageRepresentation errors out
    // when trying to create the scoped read access.
    test_image_backing->SetCleared();

    return std::move(test_image_backing);
  }
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override {
    return true;
  }
  SharedImageBackingType GetBackingType() override {
    return SharedImageBackingType::kTest;
  }
};

}  // namespace

// This mock allows individual tests to decide asynchronously when to finish a
// decode by using the FinishOneDecode() method.
class MockImageDecodeAcceleratorWorker : public ImageDecodeAcceleratorWorker {
 public:
  MockImageDecodeAcceleratorWorker(gfx::BufferFormat format_for_decodes)
      : format_for_decodes_(format_for_decodes) {}

  MockImageDecodeAcceleratorWorker(const MockImageDecodeAcceleratorWorker&) =
      delete;
  MockImageDecodeAcceleratorWorker& operator=(
      const MockImageDecodeAcceleratorWorker&) = delete;

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
      // the SharedImage backing in these tests, the only requirement is that
      // the NativePixmapHandle has the right number of planes.
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
      decode_result->buffer_byte_size = kDecodedBufferByteSize;
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

  ImageDecodeAcceleratorStubTest(const ImageDecodeAcceleratorStubTest&) =
      delete;
  ImageDecodeAcceleratorStubTest& operator=(
      const ImageDecodeAcceleratorStubTest&) = delete;

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

    // Register Skia's memory dump provider so that we can inspect its reported
    // memory usage.
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);

    // Initialize the GrContext so that texture uploading works.
    ContextResult context_result;
    scoped_refptr<SharedContextState> shared_context_state =
        channel_manager()->GetSharedContextState(&context_result);
    ASSERT_EQ(ContextResult::kSuccess, context_result);
    ASSERT_TRUE(shared_context_state);
    shared_context_state->InitializeSkia(GpuPreferences(),
                                         GpuDriverBugWorkarounds());

    GpuChannel* channel = CreateChannel(kChannelId, false /* is_gpu_host */);
    ASSERT_TRUE(channel);
    channel->shared_image_stub()
        ->factory()
        ->RegisterSharedImageBackingFactoryForTesting(&test_factory_);

    // Create a raster command buffer so that the ImageDecodeAcceleratorStub can
    // have access to a TransferBufferManager. Note that we mock the
    // MemoryTracker because GpuCommandBufferMemoryTracker uses a timer that
    // would make RunTasksUntilIdle() run forever.
    CommandBufferStub::SetMemoryTrackerFactoryForTesting(
        base::BindRepeating(&CreateMockMemoryTracker));
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = MSG_ROUTING_NONE;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->attribs.enable_gles2_interface = false;
    init_params->attribs.enable_raster_interface = true;
    init_params->attribs.bind_generates_resource = false;
    init_params->active_url = GURL();
    ContextResult result = ContextResult::kTransientFailure;
    Capabilities capabilities;
    GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kCommandBufferRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    ASSERT_EQ(ContextResult::kSuccess, result);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    ASSERT_TRUE(command_buffer);

    // Make sure there are no pending tasks before starting the test. Command
    // buffer creation creates some throw-away Mojo endpoints that will post
    // some tasks.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(task_environment().MainThreadIsIdle());
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
                                       scoped_refptr<Buffer> buffer) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    DCHECK(channel);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    CHECK(command_buffer);
    command_buffer->RegisterTransferBufferForTest(shm_id, std::move(buffer));
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
            handle.BufferForTesting()) /* closure */,
        std::vector<SyncToken>() /* sync_token_fences */,
        SyncToken(CommandBufferNamespace::GPU_IO,
                  command_buffer->command_buffer_id(), handle_release_count)));
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
                              uint64_t handle_release_count,
                              bool needs_mips = false) {
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
    auto decode_params = mojom::ScheduleImageDecodeParams::New();
    decode_params->output_size = output_size;
    decode_params->raster_decoder_route_id = kCommandBufferRouteId;
    decode_params->transfer_cache_entry_id = transfer_cache_entry_id;
    decode_params->discardable_handle_shm_id = handle.shm_id();
    decode_params->discardable_handle_shm_offset = handle.byte_offset();
    decode_params->discardable_handle_release_count = handle_release_count;
    decode_params->needs_mips = needs_mips;
    channel->GetGpuChannelForTesting().ScheduleImageDecode(
        std::move(decode_params), decode_sync_token.release_count());
    return decode_sync_token;
  }

  void RunTasksUntilIdle() { task_environment().RunUntilIdle(); }

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

  cc::ServiceImageTransferCacheEntry* RunSimpleDecode(bool needs_mips) {
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    const SyncToken decode_sync_token = SendDecodeRequest(
        gfx::Size(100, 100) /* output_size */, 1u /* decode_release_count */,
        1u /* transfer_cache_entry_id */, 1u /* handle_release_count */,
        needs_mips);
    if (!decode_sync_token.HasData())
      return nullptr;
    image_decode_accelerator_worker_.FinishOneDecode(true);
    RunTasksUntilIdle();
    if (!sync_point_manager()->IsSyncTokenReleased(decode_sync_token))
      return nullptr;
    ServiceTransferCache* transfer_cache = GetServiceTransferCache();
    if (!transfer_cache)
      return nullptr;
    const int raster_decoder_id = GetRasterDecoderId();
    if (raster_decoder_id < 0)
      return nullptr;
    auto* decode_entry = static_cast<cc::ServiceImageTransferCacheEntry*>(
        transfer_cache->GetEntry(ServiceTransferCache::EntryKey(
            raster_decoder_id, cc::TransferCacheEntryType::kImage,
            1u /* entry_id */)));
    if (!Mock::VerifyAndClear(&image_decode_accelerator_worker_))
      return nullptr;
    return decode_entry;
  }

  // Requests a |detail_level| process memory dump and checks:
  // - The total memory reported by the transfer cache.
  // - The total GPU resources memory reported by Skia. Skia memory allocator
  //   dumps that share a global allocator dump with a transfer cache entry are
  //   not counted (and we check that the Skia dump importance is less than the
  //   corresponding transfer cache dump in that case).
  // - The average transfer cache image entry byte size (this is only checked
  //   for background-level memory dumps).
  void ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail detail_level,
      uint64_t expected_total_transfer_cache_size,
      uint64_t expected_total_skia_gpu_resources_size,
      uint64_t expected_avg_image_size) {
    // Request a process memory dump.
    base::trace_event::MemoryDumpRequestArgs dump_args{};
    dump_args.dump_guid = 1234u;
    dump_args.dump_type =
        base::trace_event::MemoryDumpType::kExplicitlyTriggered;
    dump_args.level_of_detail = detail_level;
    dump_args.determinism = base::trace_event::MemoryDumpDeterminism::kForceGc;
    std::unique_ptr<base::trace_event::ProcessMemoryDump> dump;
    base::RunLoop run_loop;
    base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
        dump_args,
        base::BindOnce(
            [](std::unique_ptr<base::trace_event::ProcessMemoryDump>* out_pmd,
               base::RepeatingClosure quit_closure, bool success,
               uint64_t dump_guid,
               std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd) {
              if (success)
                *out_pmd = std::move(pmd);
              quit_closure.Run();
            },
            &dump, run_loop.QuitClosure()));
    RunTasksUntilIdle();
    run_loop.Run();

    // Check the transfer cache dumps are as expected.
    ServiceTransferCache* cache = GetServiceTransferCache();
    ASSERT_TRUE(cache);
    // This map will later allow us to answer the following question easily:
    // which transfer cache entry memory dump points to a given shared global
    // allocator dump?
    std::map<
        base::trace_event::MemoryAllocatorDumpGuid,
        std::pair<base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge,
                  base::trace_event::MemoryAllocatorDump*>>
        shared_dump_to_transfer_cache_entry_dump;
    std::string transfer_cache_dump_name =
        base::StringPrintf("gpu/transfer_cache/cache_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(cache));
    if (detail_level ==
        base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
      auto transfer_cache_dump_it =
          dump->allocator_dumps().find(transfer_cache_dump_name);
      ASSERT_NE(dump->allocator_dumps().end(), transfer_cache_dump_it);
      EXPECT_EQ(expected_total_transfer_cache_size,
                GetMemoryDumpByteSize(
                    transfer_cache_dump_it->second.get(),
                    base::trace_event::MemoryAllocatorDump::kNameSize));

      std::string avg_image_size_dump_name =
          transfer_cache_dump_name + "/avg_image_size";
      auto avg_image_size_dump_it =
          dump->allocator_dumps().find(avg_image_size_dump_name);
      ASSERT_NE(dump->allocator_dumps().end(), avg_image_size_dump_it);
      EXPECT_EQ(expected_avg_image_size,
                GetMemoryDumpByteSize(avg_image_size_dump_it->second.get(),
                                      "average_size"));
    } else {
      DCHECK_EQ(base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
                detail_level);
      base::CheckedNumeric<uint64_t> safe_actual_transfer_cache_total_size(0u);
      std::string entry_dump_prefix =
          transfer_cache_dump_name + "/gpu/entry_0x";
      for (const auto& allocator_dump : dump->allocator_dumps()) {
        if (base::StartsWith(allocator_dump.first, entry_dump_prefix,
                             base::CompareCase::SENSITIVE)) {
          ASSERT_TRUE(allocator_dump.second);
          safe_actual_transfer_cache_total_size += GetMemoryDumpByteSize(
              allocator_dump.second.get(),
              base::trace_event::MemoryAllocatorDump::kNameSize);

          // If the dump name for this entry does not end in /dma_buf (i.e., we
          // haven't requested mipmaps from Skia), the allocator dump for this
          // cache entry should point to a shared global allocator dump (i.e.,
          // shared with Skia). Let's save this association in
          // |shared_dump_to_transfer_cache_entry_dump| for later.
          ASSERT_FALSE(allocator_dump.second->guid().empty());
          auto edge_it =
              dump->allocator_dumps_edges().find(allocator_dump.second->guid());
          ASSERT_EQ(base::EndsWith(allocator_dump.first, "/dma_buf",
                                   base::CompareCase::SENSITIVE),
                    dump->allocator_dumps_edges().end() == edge_it);
          if (edge_it != dump->allocator_dumps_edges().end()) {
            ASSERT_FALSE(edge_it->second.target.empty());
            ASSERT_EQ(shared_dump_to_transfer_cache_entry_dump.end(),
                      shared_dump_to_transfer_cache_entry_dump.find(
                          edge_it->second.target));
            shared_dump_to_transfer_cache_entry_dump[edge_it->second.target] =
                std::make_pair(edge_it->second, allocator_dump.second.get());
          }
        }
      }
      ASSERT_TRUE(safe_actual_transfer_cache_total_size.IsValid());
      EXPECT_EQ(expected_total_transfer_cache_size,
                safe_actual_transfer_cache_total_size.ValueOrDie());
    }

    // Check that the Skia dumps are as expected. We won't count Skia dumps that
    // point to a global allocator dump that's shared with a transfer cache
    // dump.
    base::CheckedNumeric<uint64_t> safe_actual_total_skia_gpu_resources_size(
        0u);
    for (const auto& allocator_dump : dump->allocator_dumps()) {
      if (base::StartsWith(allocator_dump.first, "skia/gpu_resources",
                           base::CompareCase::SENSITIVE)) {
        ASSERT_TRUE(allocator_dump.second);
        uint64_t skia_allocator_dump_size = GetMemoryDumpByteSize(
            allocator_dump.second.get(),
            base::trace_event::MemoryAllocatorDump::kNameSize);

        // If this dump points to a global allocator dump that's shared with a
        // transfer cache dump, we won't count it.
        ASSERT_FALSE(allocator_dump.second->guid().empty());
        auto edge_it =
            dump->allocator_dumps_edges().find(allocator_dump.second->guid());
        if (edge_it != dump->allocator_dumps_edges().end()) {
          ASSERT_FALSE(edge_it->second.target.empty());
          auto transfer_cache_dump_it =
              shared_dump_to_transfer_cache_entry_dump.find(
                  edge_it->second.target);
          if (transfer_cache_dump_it !=
              shared_dump_to_transfer_cache_entry_dump.end()) {
            // Not counting the Skia dump is only valid if its importance is
            // less than the transfer cache dump and the values of the dumps are
            // the same.
            EXPECT_EQ(skia_allocator_dump_size,
                      GetMemoryDumpByteSize(
                          transfer_cache_dump_it->second.second,
                          base::trace_event::MemoryAllocatorDump::kNameSize));
            EXPECT_LT(edge_it->second.importance,
                      transfer_cache_dump_it->second.first.importance);
            continue;
          }
        }

        safe_actual_total_skia_gpu_resources_size += skia_allocator_dump_size;
      }
    }
    ASSERT_TRUE(safe_actual_total_skia_gpu_resources_size.IsValid());
    EXPECT_EQ(expected_total_skia_gpu_resources_size,
              safe_actual_total_skia_gpu_resources_size.ValueOrDie());
  }

 protected:
  StrictMock<MockImageDecodeAcceleratorWorker> image_decode_accelerator_worker_;

 private:
  TestSharedImageBackingFactory test_factory_;
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<ImageDecodeAcceleratorStubTest> weak_ptr_factory_{this};
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
  sync_point_manager()->set_suppress_fatal_log_for_testing();
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
  sync_point_manager()->set_suppress_fatal_log_for_testing();
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

TEST_P(ImageDecodeAcceleratorStubTest, MemoryReportDetailedForUnmippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(false /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
      base::strict_cast<uint64_t>(
          kDecodedBufferByteSize) /* expected_total_transfer_cache_size */,
      0u /* expected_total_skia_gpu_resources_size */,
      0u /* expected_avg_image_size */);
}

TEST_P(ImageDecodeAcceleratorStubTest,
       MemoryReportBackgroundForUnmippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(false /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      base::strict_cast<uint64_t>(
          kDecodedBufferByteSize) /* expected_total_transfer_cache_size */,
      0u /* expected_total_skia_gpu_resources_size */,
      base::strict_cast<uint64_t>(
          kDecodedBufferByteSize) /* expected_avg_image_size */);
}

TEST_P(ImageDecodeAcceleratorStubTest, MemoryReportDetailedForMippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(true /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetParam()),
            decode_entry->plane_images().size());
  base::CheckedNumeric<uint64_t> safe_expected_total_transfer_cache_size =
      GetExpectedTotalMippedSizeForPlanarImage(decode_entry);
  ASSERT_TRUE(safe_expected_total_transfer_cache_size.IsValid());
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
      safe_expected_total_transfer_cache_size.ValueOrDie(),
      kSkiaBufferObjectSize /* expected_total_skia_gpu_resources_size */,
      0u /* expected_avg_image_size */);
}

TEST_P(ImageDecodeAcceleratorStubTest, MemoryReportBackgroundForMippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(true /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetParam()),
            decode_entry->plane_images().size());
  base::CheckedNumeric<uint64_t> safe_expected_total_transfer_cache_size =
      GetExpectedTotalMippedSizeForPlanarImage(decode_entry);
  ASSERT_TRUE(safe_expected_total_transfer_cache_size.IsValid());
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      safe_expected_total_transfer_cache_size.ValueOrDie(),
      kSkiaBufferObjectSize,
      safe_expected_total_transfer_cache_size
          .ValueOrDie() /* expected_avg_image_size */);
}

TEST_P(ImageDecodeAcceleratorStubTest,
       MemoryReportDetailedForDeferredMippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(false /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  decode_entry->EnsureMips();
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetParam()),
            decode_entry->plane_images().size());
  base::CheckedNumeric<uint64_t> safe_expected_total_transfer_cache_size =
      GetExpectedTotalMippedSizeForPlanarImage(decode_entry);
  ASSERT_TRUE(safe_expected_total_transfer_cache_size.IsValid());
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
      safe_expected_total_transfer_cache_size.ValueOrDie(),
      kSkiaBufferObjectSize /* expected_total_skia_gpu_resources_size */,
      0u /* expected_avg_image_size */);
}

TEST_P(ImageDecodeAcceleratorStubTest,
       MemoryReportBackgroundForDeferredMippedDecode) {
  cc::ServiceImageTransferCacheEntry* decode_entry =
      RunSimpleDecode(false /* needs_mips */);
  ASSERT_TRUE(decode_entry);
  decode_entry->EnsureMips();
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetParam()),
            decode_entry->plane_images().size());
  // For a deferred mip request, the transfer cache doesn't update its size
  // computation, so it reports memory as if no mips had been generated.
  ExpectProcessMemoryDump(
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      base::strict_cast<uint64_t>(
          kDecodedBufferByteSize) /* expected_total_transfer_cache_size */,
      kSkiaBufferObjectSize,
      base::strict_cast<uint64_t>(
          kDecodedBufferByteSize) /* expected_avg_image_size */);
}

// TODO(andrescj): test the deletion of transfer cache entries.

INSTANTIATE_TEST_SUITE_P(
    All,
    ImageDecodeAcceleratorStubTest,
    ::testing::Values(gfx::BufferFormat::YVU_420,
                      gfx::BufferFormat::YUV_420_BIPLANAR));

}  // namespace gpu
