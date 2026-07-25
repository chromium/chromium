// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_service_serializer.h"

#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "ipc/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace gpu::webgpu {

namespace {

constexpr size_t kWireAlignment = 8u;
constexpr size_t kMaxWireBufferSize =
    std::min(static_cast<size_t>(IPC::mojom::kChannelMaximumMessageSize),
             static_cast<size_t>(1024 * 1024));

constexpr size_t kDawnReturnCmdsOffset =
    offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer);

static_assert(kDawnReturnCmdsOffset < kMaxWireBufferSize, "");

}  // anonymous namespace

DawnServiceSerializer::CommandBuffer::CommandBuffer(size_t size)
    : buffer(base::AlignedUninit<std::byte>(size, kWireAlignment)),
      put_offset(kDawnReturnCmdsOffset) {
  // Zero-initialize the entire buffer because wire command serialization
  // does not initialize struct alignment padding or trailing padding bytes.
  std::ranges::fill(buffer, std::byte{0});
  cmds::DawnReturnCommandsInfoHeader* header =
      reinterpret_cast<cmds::DawnReturnCommandsInfoHeader*>(&buffer[0]);
  header->return_data_header.return_data_type =
      DawnReturnDataType::kDawnCommands;
}

DawnServiceSerializer::CommandBuffer::~CommandBuffer() = default;

// Thread local boolean used to determine whether a thread is still recording
// data. We need to use thread_local here because the calling thread might not
// be a Dawn/Chromium managed thread, i.e. a Metal thread.
static constinit thread_local bool current_thread_pending_flush = false;

DawnServiceSerializer::DawnServiceSerializer(DecoderClient* client)
    : gpu_main_thread_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      client_(client),
      main_cmds_(kMaxWireBufferSize),
      worker_cv_(&worker_lock_),
      worker_cmds_(kMaxWireBufferSize) {}

DawnServiceSerializer::~DawnServiceSerializer() = default;

size_t DawnServiceSerializer::GetMaximumAllocationSize() const {
  return kMaxWireBufferSize - kDawnReturnCmdsOffset;
}

bool DawnServiceSerializer::NeedsFlush() const {
  DCHECK(gpu_main_thread_runner_->BelongsToCurrentThread());
  return main_cmds_.put_offset > kDawnReturnCmdsOffset;
}

bool DawnServiceSerializer::SetWorkerPending(bool pending) {
  worker_lock_.AssertAcquired();
  if (pending == current_thread_pending_flush) {
    return false;
  }

  if (pending) {
    current_thread_pending_flush = true;
    pending_workers_ += 1;
    DCHECK_GT(pending_workers_, 0u);
  } else {
    current_thread_pending_flush = false;
    DCHECK_GT(pending_workers_, 0u);
    pending_workers_ -= 1;
  }
  return true;
}

std::optional<std::span<volatile std::byte>>
DawnServiceSerializer::GetCommandSpace(size_t size) {
  if (gpu_main_thread_runner_->BelongsToCurrentThread()) {
    return GetCommandSpaceMain(size);
  } else {
    return GetCommandSpaceWorker(size);
  }
}

std::optional<std::span<volatile std::byte>>
DawnServiceSerializer::GetCommandSpace(CommandBuffer& cmd_buffer, size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(cmd_buffer.put_offset, kMaxWireBufferSize);
  DCHECK_LE(size, GetMaximumAllocationSize());

  // Statically check that kMaxWireBufferSize + kMaxWireBufferSize is
  // a valid uint32_t. We can add put_offset_ and size without overflow.
  static_assert(base::CheckAdd(kMaxWireBufferSize, kMaxWireBufferSize)
                    .IsValid<uint32_t>(),
                "");

  uint32_t next_offset = cmd_buffer.put_offset + static_cast<uint32_t>(size);
  if (next_offset > cmd_buffer.buffer.size()) {
    return std::nullopt;
  }

  auto space = cmd_buffer.buffer.subspan(cmd_buffer.put_offset, size);
  cmd_buffer.put_offset = next_offset;
  return std::span<std::byte>(space.data(), space.size());
}

std::optional<std::span<volatile std::byte>>
DawnServiceSerializer::GetCommandSpaceMain(size_t size) {
  DCHECK(gpu_main_thread_runner_->BelongsToCurrentThread());

  auto span = GetCommandSpace(main_cmds_, size);
  if (!span) {
    FlushMain();
    span = GetCommandSpace(main_cmds_, size);
  }

  CHECK(span);
  return span;
}

std::optional<std::span<volatile std::byte>>
DawnServiceSerializer::GetCommandSpaceWorker(size_t size) {
  DCHECK(!gpu_main_thread_runner_->BelongsToCurrentThread());

  std::optional<std::span<volatile std::byte>> span;
  {
    base::AutoLock guard(worker_lock_);
    while (!(span = GetCommandSpace(worker_cmds_, size))) {
      // When a thread requests for more space, we can assume that it has
      // completed recording any previous space it requested. So if we run out
      // of space and need to flush, we can decrement the number of threads
      // still pending, issue a flush, and wait.
      if (SetWorkerPending(false)) {
        // Note that the FlushWorker call below won't do anything until
        // |pending_workers_| == 0. If multiple threads are pending and fail,
        // they will all end up in |Wait| until no worker is pending anymore,
        // and the last worker to leave the pending state will result in a
        // successful flush at which point the threads will be woken up via
        // |Signal| one at a time.
        gpu_main_thread_runner_->PostTask(
            FROM_HERE, base::BindOnce(&DawnServiceSerializer::FlushWorker,
                                      weak_ptr_factory_.GetWeakPtr()));
      }
      worker_cv_.Wait();
    }
    SetWorkerPending(true);
  }
  worker_cv_.Signal();

  CHECK(span);
  return span;
}

void DawnServiceSerializer::Flush(CommandBuffer& cmd_buffer) {
  if (cmd_buffer.put_offset <= kDawnReturnCmdsOffset) {
    return;
  }

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "DawnServiceSerializer::Flush", "bytes", cmd_buffer.put_offset);

  bool is_tracing = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                                     &is_tracing);
  if (is_tracing) {
    uint64_t trace_id = base::RandUint64();
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnReturnCommands",
                perfetto::Flow::Global(trace_id));
    cmds::DawnReturnCommandsInfoHeader* header =
        reinterpret_cast<cmds::DawnReturnCommandsInfoHeader*>(
            &cmd_buffer.buffer[0]);
    header->return_data_header.trace_id = trace_id;
  }

  client_->HandleReturnData(
      base::as_bytes(cmd_buffer.buffer.first(cmd_buffer.put_offset)));
  cmd_buffer.put_offset = kDawnReturnCmdsOffset;
}

void DawnServiceSerializer::FlushMain() {
  DCHECK(gpu_main_thread_runner_->BelongsToCurrentThread());
  Flush(main_cmds_);
}

void DawnServiceSerializer::FlushWorker() {
  DCHECK(gpu_main_thread_runner_->BelongsToCurrentThread());
  {
    base::AutoLock guard(worker_lock_);
    if (pending_workers_ > 0) {
      // If we still have pending workers, we can assume that those workers also
      // will eventually cause a flush and we can handle the actual flushing
      // then.
      return;
    }
    Flush(worker_cmds_);
  }
  worker_cv_.Signal();
}

bool DawnServiceSerializer::Flush() {
  if (gpu_main_thread_runner_->BelongsToCurrentThread()) {
    FlushMain();
  } else {
    {
      // Update the number of threads that are ready to be actually flushed.
      base::AutoLock guard(worker_lock_);
      SetWorkerPending(false);
    }
    gpu_main_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DawnServiceSerializer::FlushWorker,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  return true;
}

}  //  namespace gpu::webgpu
