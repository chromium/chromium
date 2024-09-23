// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/common_decoder.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>

#include "base/numerics/safe_math.h"
#include "base/pickle.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace gpu {
namespace {
static const size_t kDefaultMaxBucketSize = 1u << 30;  // 1 GB

uint32_t LoadU32Unaligned(const void* ptr) {
  uint32_t ret;
  memcpy(&ret, ptr, sizeof(uint32_t));
  return ret;
}

void StoreU32Unaligned(uint32_t v, void* ptr) {
  memcpy(ptr, &v, sizeof(uint32_t));
}

}  // namespace

const CommonDecoder::CommandInfo CommonDecoder::command_info[] = {
#define COMMON_COMMAND_BUFFER_CMD_OP(name)                       \
  {                                                              \
    &CommonDecoder::Handle##name, cmd::name::kArgFlags,          \
        cmd::name::cmd_flags,                                    \
        sizeof(cmd::name) / sizeof(CommandBufferEntry) - 1,      \
  }                                                              \
  ,  /* NOLINT */
  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)
  #undef COMMON_COMMAND_BUFFER_CMD_OP
};


CommonDecoder::Bucket::Bucket() : size_(0) {}

CommonDecoder::Bucket::~Bucket() = default;

void* CommonDecoder::Bucket::GetData(size_t offset, size_t size) const {
  if (OffsetSizeValid(offset, size)) {
    return data_.get() + offset;
  }
  return nullptr;
}

void CommonDecoder::Bucket::SetSize(size_t size) {
  if (size != size_) {
    // Note: the `()` after `new[]` is significant: it ensures the elements are
    // value-initialized (not to be confused with default *initialized*). In the
    // case of int8_t, that means the returned buffer will be
    // zero-initialized.
    data_.reset(size ? new int8_t[size]() : nullptr);
    size_ = size;
  }
}

bool CommonDecoder::Bucket::SetData(
    const volatile void* src, size_t offset, size_t size) {
  if (OffsetSizeValid(offset, size)) {
    memcpy(data_.get() + offset, const_cast<const void*>(src), size);
    return true;
  }
  return false;
}

void CommonDecoder::Bucket::SetFromString(const char* str) {
  // Strings are passed nullptr terminated to distinguish between empty string
  // and no string.
  if (!str) {
    SetSize(0);
  } else {
    size_t size = strlen(str) + 1;
    SetSize(size);
    SetData(str, 0, size);
  }
}

bool CommonDecoder::Bucket::GetAsString(std::string* str) {
  DCHECK(str);
  if (size_ == 0) {
    return false;
  }
  str->assign(GetDataAs<const char*>(0, size_ - 1), size_ - 1);
  return true;
}

bool CommonDecoder::Bucket::GetAsStrings(
    GLsizei* _count, std::vector<char*>* _string, std::vector<GLint>* _length) {
  const size_t kMinBucketSize = sizeof(GLint);
  // Each string has at least |length| in the header and a NUL character.
  const size_t kMinStringSize = sizeof(GLint) + 1;
  const size_t bucket_size = this->size();
  if (bucket_size < kMinBucketSize) {
    return false;
  }
  char* bucket_data = this->GetDataAs<char*>(0, bucket_size);
  GLint* header = reinterpret_cast<GLint*>(bucket_data);
  GLsizei count = static_cast<GLsizei>(header[0]);
  if (count < 0) {
    return false;
  }
  const size_t max_count = (bucket_size - kMinBucketSize) / kMinStringSize;
  if (max_count < static_cast<size_t>(count)) {
    return false;
  }
  GLint* length = header + 1;
  std::vector<char*> strs(count);
  base::CheckedNumeric<size_t> total_size = sizeof(GLint);
  total_size *= count + 1;  // Header size.
  if (!total_size.IsValid())
    return false;
  for (GLsizei ii = 0; ii < count; ++ii) {
    strs[ii] = bucket_data + total_size.ValueOrDefault(0);
    total_size += length[ii];
    total_size += 1;  // NUL char at the end of each char array.
    if (!total_size.IsValid() || total_size.ValueOrDefault(0) > bucket_size ||
        strs[ii][length[ii]] != 0) {
      return false;
    }
  }
  if (total_size.ValueOrDefault(0) != bucket_size) {
    return false;
  }
  DCHECK(_count && _string && _length);
  *_count = count;
  *_string = strs;
  _length->resize(count);
  for (GLsizei ii = 0; ii < count; ++ii) {
    (*_length)[ii] = length[ii];
  }
  return true;
}

bool CommonDecoder::Bucket::OffsetSizeValid(size_t offset, size_t size) const {
  size_t end = 0;
  if (!base::CheckAdd<size_t>(offset, size).AssignIfValid(&end))
    return false;
  return end <= size_;
}

CommonDecoder::CommonDecoder(DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service)
    : command_buffer_service_(command_buffer_service),
      client_(client),
      max_bucket_size_(kDefaultMaxBucketSize) {
  DCHECK(command_buffer_service_);
}

CommonDecoder::~CommonDecoder() = default;

void* CommonDecoder::GetAddressAndCheckSize(unsigned int shm_id,
                                            unsigned int data_offset,
                                            unsigned int data_size) {
  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->GetTransferBuffer(shm_id);
  if (!buffer.get())
    return nullptr;
  return buffer->GetDataAddress(data_offset, data_size);
}

void* CommonDecoder::GetAddressAndSize(unsigned int shm_id,
                                       unsigned int data_offset,
                                       unsigned int minimum_size,
                                       unsigned int* data_size) {
  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->GetTransferBuffer(shm_id);
  if (!buffer.get() || buffer->GetRemainingSize(data_offset) < minimum_size)
    return nullptr;
  return buffer->GetDataAddressAndSize(data_offset, data_size);
}

unsigned int CommonDecoder::GetSharedMemorySize(unsigned int shm_id,
                                                unsigned int offset) {
  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->GetTransferBuffer(shm_id);
  if (!buffer.get())
    return 0;
  return buffer->GetRemainingSize(offset);
}

scoped_refptr<gpu::Buffer> CommonDecoder::GetSharedMemoryBuffer(
    unsigned int shm_id) {
  return command_buffer_service_->GetTransferBuffer(shm_id);
}

const char* CommonDecoder::GetCommonCommandName(
    cmd::CommandId command_id) const {
  return cmd::GetCommandName(command_id);
}

CommonDecoder::Bucket* CommonDecoder::GetBucket(uint32_t bucket_id) const {
  BucketMap::const_iterator iter(buckets_.find(bucket_id));
  return iter != buckets_.end() ? &(*iter->second) : nullptr;
}

CommonDecoder::Bucket* CommonDecoder::CreateBucket(uint32_t bucket_id) {
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    bucket = new Bucket();
    buckets_[bucket_id] = std::unique_ptr<Bucket>(bucket);
  }
  return bucket;
}

namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const volatile void* AddressAfterStruct(const volatile T& pod) {
  return reinterpret_cast<const volatile uint8_t*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const volatile COMMAND_TYPE& pod) {
  return static_cast<RETURN_TYPE>(
      const_cast<volatile void*>(AddressAfterStruct(pod)));
}

}  // anonymous namespace.

// Decode command with its arguments, and call the corresponding method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
error::Error CommonDecoder::DoCommonCommand(unsigned int command,
                                            unsigned int arg_count,
                                            const volatile void* cmd_data) {
  if (command < std::size(command_info)) {
    const CommandInfo& info = command_info[command];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      uint32_t immediate_data_size =
          (arg_count - info_arg_count) * sizeof(CommandBufferEntry);  // NOLINT
      return (this->*info.cmd_handler)(immediate_data_size, cmd_data);
    } else {
      return error::kInvalidArguments;
    }
  }
  return error::kUnknownCommand;
}

error::Error CommonDecoder::HandleNoop(uint32_t immediate_data_size,
                                       const volatile void* cmd_data) {
  return error::kNoError;
}

error::Error CommonDecoder::HandleSetToken(uint32_t immediate_data_size,
                                           const volatile void* cmd_data) {
  const volatile cmd::SetToken& args =
      *static_cast<const volatile cmd::SetToken*>(cmd_data);
  command_buffer_service_->SetToken(args.token);
  return error::kNoError;
}

error::Error CommonDecoder::HandleSetBucketSize(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile cmd::SetBucketSize& args =
      *static_cast<const volatile cmd::SetBucketSize*>(cmd_data);
  uint32_t bucket_id = args.bucket_id;
  uint32_t size = args.size;
  if (size > max_bucket_size_)
    return error::kOutOfBounds;

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(size);
  return error::kNoError;
}

error::Error CommonDecoder::HandleSetBucketData(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile cmd::SetBucketData& args =
      *static_cast<const volatile cmd::SetBucketData*>(cmd_data);
  uint32_t bucket_id = args.bucket_id;
  uint32_t offset = args.offset;
  uint32_t size = args.size;
  const void* data = GetSharedMemoryAs<const void*>(
      args.shared_memory_id, args.shared_memory_offset, size);
  if (!data) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  if (!bucket->SetData(data, offset, size)) {
    return error::kInvalidArguments;
  }

  return error::kNoError;
}

error::Error CommonDecoder::HandleSetBucketDataImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmd::SetBucketDataImmediate& args =
      *static_cast<const volatile cmd::SetBucketDataImmediate*>(cmd_data);
  const volatile void* data = GetImmediateDataAs<const volatile void*>(args);
  uint32_t bucket_id = args.bucket_id;
  uint32_t offset = args.offset;
  uint32_t size = args.size;
  if (size > immediate_data_size) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  if (!bucket->SetData(data, offset, size)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error CommonDecoder::HandleGetBucketStart(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmd::GetBucketStart& args =
      *static_cast<const volatile cmd::GetBucketStart*>(cmd_data);
  uint32_t bucket_id = args.bucket_id;
  // `result` may not be aligned, so cast to `void*` and use `memcpy` to load
  // and store.
  void* result = GetSharedMemoryAs<void*>(
      args.result_memory_id, args.result_memory_offset, sizeof(uint32_t));
  int32_t data_memory_id = args.data_memory_id;
  uint32_t data_memory_offset = args.data_memory_offset;
  uint32_t data_memory_size = args.data_memory_size;
  uint8_t* data = nullptr;
  if (data_memory_size != 0 || data_memory_id != 0 || data_memory_offset != 0) {
    data = GetSharedMemoryAs<uint8_t*>(data_memory_id, data_memory_offset,
                                       data_memory_size);
    if (!data) {
      return error::kInvalidArguments;
    }
  }
  if (!result) {
    return error::kInvalidArguments;
  }
  // Check that the client initialized the result.
  if (LoadU32Unaligned(result) != 0) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32_t bucket_size = bucket->size();
  StoreU32Unaligned(bucket_size, result);
  if (data) {
    uint32_t size = std::min(data_memory_size, bucket_size);
    memcpy(data, bucket->GetData(0, size), size);
  }
  return error::kNoError;
}

error::Error CommonDecoder::HandleGetBucketData(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile cmd::GetBucketData& args =
      *static_cast<const volatile cmd::GetBucketData*>(cmd_data);
  uint32_t bucket_id = args.bucket_id;
  uint32_t offset = args.offset;
  uint32_t size = args.size;
  void* data = GetSharedMemoryAs<void*>(
      args.shared_memory_id, args.shared_memory_offset, size);
  if (!data) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  const void* src = bucket->GetData(offset, size);
  if (!src) {
      return error::kInvalidArguments;
  }
  memcpy(data, src, size);
  return error::kNoError;
}

error::Error CommonDecoder::HandleInsertFenceSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmd::InsertFenceSync& c =
      *static_cast<const volatile cmd::InsertFenceSync*>(cmd_data);

  const uint64_t release_count = c.release_count();
  client_->OnFenceSyncRelease(release_count);
  // Exit inner command processing loop so that we check the scheduling state
  // and yield if necessary as we may have unblocked a higher priority
  // context.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

bool CommonDecoder::ReadColorSpace(uint32_t shm_id,
                                   uint32_t shm_offset,
                                   uint32_t color_space_size,
                                   gfx::ColorSpace* color_space) {
  // Use the default (invalid) color space if no space was serialized.
  if (!shm_id && !shm_offset && !color_space_size) {
    *color_space = gfx::ColorSpace();
    return true;
  }

  const uint8_t* data = static_cast<const uint8_t*>(
      GetAddressAndCheckSize(shm_id, shm_offset, color_space_size));
  if (!data) {
    return false;
  }

  base::span<const uint8_t> color_space_data(data, data + color_space_size);
  base::Pickle color_space_pickle =
      base::Pickle::WithUnownedBuffer(color_space_data);
  base::PickleIterator iterator(color_space_pickle);
  if (!IPC::ParamTraits<gfx::ColorSpace>::Read(&color_space_pickle, &iterator,
                                               color_space)) {
    return false;
  }
  return true;
}

}  // namespace gpu
