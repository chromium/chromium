// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tools/fuzzers/mojolpm.h"

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojolpm {

const uint32_t kPipeElementMaxSize = 0x1000u;
const uint32_t kPipeCapacityMaxSize = 0x100000u;
const uint32_t kPipeActionMaxSize = 0x100000u;

const uint64_t kSharedBufferMaxSize = 0x100000u;
const uint32_t kSharedBufferActionMaxSize = 0x100000u;

Context::Context() = default;

Context::~Context() = default;

Context::Storage::Storage() = default;

Context::Storage::~Storage() = default;

void Context::StartTestcase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Context::EndTestcase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Some fuzzers need to destroy the fuzzer thread along with their testcase,
  // so we need to detach the sequence checker here so that it will be attached
  // to the new sequence for the next testcase.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // We need to destroy all Remotes/Receivers before we start destroying other
  // objects (like callbacks).
  for (const TypeId& interface_type_id : interface_type_ids_) {
    auto instances_iter = instances_.find(interface_type_id);
    if (instances_iter != instances_.end()) {
      instances_iter->second.clear();
    }
  }
  interface_type_ids_.clear();
  instances_.clear();
}

void Context::StartDeserialization() {
  rollback_.clear();
}

void Context::EndDeserialization(Rollback rollback) {
  if (rollback == Rollback::kRollback) {
    for (const auto& entry : rollback_) {
      RemoveInstance(entry.first, entry.second);
    }
  }
  rollback_.clear();
}

void Context::RemoveInstance(TypeId type_id, uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto instances_iter = instances_.find(type_id);
  if (instances_iter != instances_.end()) {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return;
    }

    instance_map.erase(instance);
  } else {
    mojolpmdbg("failed!\n");
  }
}

Context* GetContext() {
  static base::NoDestructor<Context> context;
  return context.get();
}

bool FromProto(const bool& input, bool& output) {
  output = input;
  return true;
}

bool ToProto(const bool& input, bool& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int8_t& output) {
  output = input;
  return true;
}

bool ToProto(const int8_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int16_t& output) {
  output = input;
  return true;
}

bool ToProto(const int16_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int32& input, int32_t& output) {
  output = input;
  return true;
}

bool ToProto(const int32_t& input, ::google::protobuf::int32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::int64& input, int64_t& output) {
  output = input;
  return true;
}

bool ToProto(const int64_t& input, ::google::protobuf::int64& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint8_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint8_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint16_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint16_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint32& input, uint32_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint32_t& input, ::google::protobuf::uint32& output) {
  output = input;
  return true;
}

bool FromProto(const ::google::protobuf::uint64& input, uint64_t& output) {
  output = input;
  return true;
}

bool ToProto(const uint64_t& input, ::google::protobuf::uint64& output) {
  output = input;
  return true;
}

bool FromProto(const double& input, double& output) {
  output = input;
  return true;
}

bool ToProto(const double& input, double& output) {
  output = input;
  return true;
}

bool FromProto(const float& input, float& output) {
  output = input;
  return true;
}

bool ToProto(const float& input, float& output) {
  output = input;
  return true;
}

bool FromProto(const std::string& input, std::string& output) {
  output = input;
  return true;
}

bool ToProto(const std::string& input, std::string& output) {
  output = input;
  return true;
}

bool FromProto(const ::mojolpm::Handle& input, mojo::ScopedHandle& output) {
  return true;
}

bool ToProto(const mojo::ScopedHandle& input, ::mojolpm::Handle& output) {
  return true;
}

bool FromProto(const ::mojolpm::DataPipeConsumerHandle& input,
               mojo::ScopedDataPipeConsumerHandle& output) {
  bool result = false;

  if (input.instance_case() == ::mojolpm::DataPipeConsumerHandle::kOld) {
    auto old = mojolpm::GetContext()
                   ->GetAndRemoveInstance<mojo::ScopedDataPipeConsumerHandle>(
                       input.old());
    if (old) {
      output = std::move(*old.release());
    }
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.new_().flags();
    options.element_num_bytes =
        std::min(input.new_().element_num_bytes(), kPipeElementMaxSize);
    options.capacity_num_bytes =
        std::min(input.new_().capacity_num_bytes(), kPipeCapacityMaxSize);

    if (MOJO_RESULT_OK == mojo::CreateDataPipe(&options, producer, consumer)) {
      result = true;
      output = std::move(consumer);
      mojolpm::GetContext()->AddInstance(std::move(producer));
    }
  }

  return result;
}

bool ToProto(const mojo::ScopedDataPipeConsumerHandle& input,
             ::mojolpm::DataPipeConsumerHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::DataPipeProducerHandle& input,
               mojo::ScopedDataPipeProducerHandle& output) {
  bool result = false;

  if (input.instance_case() == ::mojolpm::DataPipeProducerHandle::kOld) {
    auto old = mojolpm::GetContext()
                   ->GetAndRemoveInstance<mojo::ScopedDataPipeProducerHandle>(
                       input.old());
    if (old) {
      output = std::move(*old.release());
    }
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.new_().flags();
    options.element_num_bytes =
        std::min(input.new_().element_num_bytes(), kPipeElementMaxSize);
    options.capacity_num_bytes =
        std::min(input.new_().capacity_num_bytes(), kPipeCapacityMaxSize);

    if (MOJO_RESULT_OK == mojo::CreateDataPipe(&options, producer, consumer)) {
      result = true;
      output = std::move(producer);
      mojolpm::GetContext()->AddInstance(std::move(consumer));
    }
  }

  return result;
}

bool ToProto(const mojo::ScopedDataPipeProducerHandle& input,
             ::mojolpm::DataPipeProducerHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::MessagePipeHandle& input,
               mojo::ScopedMessagePipeHandle& output) {
  return true;
}

bool ToProto(const mojo::ScopedMessagePipeHandle& input,
             ::mojolpm::MessagePipeHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::SharedBufferHandle& input,
               mojo::ScopedSharedBufferHandle& output) {
  bool result = false;

  if (input.instance_case() == ::mojolpm::SharedBufferHandle::kOld) {
    auto old =
        mojolpm::GetContext()
            ->GetAndRemoveInstance<mojo::ScopedSharedBufferHandle>(input.old());
    if (old) {
      output = std::move(*old.release());
    }
  } else {
    output = mojo::SharedBufferHandle::Create(
        std::min(input.new_().num_bytes(), kSharedBufferMaxSize));
    result = true;
  }

  return result;
}

bool ToProto(const mojo::ScopedSharedBufferHandle& input,
             ::mojolpm::SharedBufferHandle& output) {
  return true;
}

bool FromProto(const ::mojolpm::PlatformHandle& input,
               mojo::PlatformHandle& output) {
  return true;
}

bool ToProto(const mojo::PlatformHandle& input,
             ::mojolpm::PlatformHandle& output) {
  return true;
}

void HandleDataPipeRead(const ::mojolpm::DataPipeRead& input) {
  mojo::ScopedDataPipeConsumerHandle* consumer_ptr = nullptr;

  if (input.handle().instance_case() ==
      ::mojolpm::DataPipeConsumerHandle::kOld) {
    consumer_ptr =
        mojolpm::GetContext()->GetInstance<mojo::ScopedDataPipeConsumerHandle>(
            input.handle().old());
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.handle().new_().flags();
    options.element_num_bytes = std::min(
        input.handle().new_().element_num_bytes(), kPipeElementMaxSize);
    options.capacity_num_bytes = std::min(
        input.handle().new_().capacity_num_bytes(), kPipeCapacityMaxSize);

    if (MOJO_RESULT_OK == mojo::CreateDataPipe(&options, producer, consumer)) {
      int id = mojolpm::GetContext()->AddInstance(std::move(consumer));
      mojolpm::GetContext()->AddInstance(std::move(producer));
      consumer_ptr = mojolpm::GetContext()
                         ->GetInstance<mojo::ScopedDataPipeConsumerHandle>(id);
    }
  }

  if (consumer_ptr) {
    size_t size = size_t{input.size()};
    if (size > kPipeActionMaxSize) {
      size = kPipeActionMaxSize;
    }
    std::vector<uint8_t> data(size);
    size_t bytes_read = 0;
    consumer_ptr->get().ReadData(MOJO_READ_DATA_FLAG_NONE, data, bytes_read);
  }
}

void HandleDataPipeWrite(const ::mojolpm::DataPipeWrite& input) {
  mojo::ScopedDataPipeProducerHandle* producer_ptr = nullptr;

  if (input.handle().instance_case() ==
      ::mojolpm::DataPipeProducerHandle::kOld) {
    producer_ptr =
        mojolpm::GetContext()->GetInstance<mojo::ScopedDataPipeProducerHandle>(
            input.handle().old());
  } else {
    MojoCreateDataPipeOptions options;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;

    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = input.handle().new_().flags();
    options.element_num_bytes = std::min(
        input.handle().new_().element_num_bytes(), kPipeElementMaxSize);
    options.capacity_num_bytes = std::min(
        input.handle().new_().capacity_num_bytes(), kPipeCapacityMaxSize);

    if (MOJO_RESULT_OK == mojo::CreateDataPipe(&options, producer, consumer)) {
      mojolpm::GetContext()->AddInstance(std::move(consumer));
      int id = mojolpm::GetContext()->AddInstance(std::move(producer));
      producer_ptr = mojolpm::GetContext()
                         ->GetInstance<mojo::ScopedDataPipeProducerHandle>(id);
    }
  }

  if (producer_ptr) {
    size_t size = input.data().size();
    if (size > kPipeActionMaxSize) {
      size = kPipeActionMaxSize;
    }
    size_t bytes_written = 0;
    producer_ptr->get().WriteData(base::as_byte_span(input.data()),
                                  MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  }
}

void HandleSharedBufferWrite(const ::mojolpm::SharedBufferWrite& input) {
  mojo::ScopedSharedBufferHandle handle;
  if (!FromProto(input.handle(), handle)) {
    return;
  }
  size_t size = input.data().size();
  if (size > kSharedBufferActionMaxSize) {
    size = kSharedBufferActionMaxSize;
  }
  size = std::min(handle->GetSize(), static_cast<uint64_t>(size));
  auto mem = handle->Map(size);
  if (!mem) {
    return;
  }
  std::memcpy(mem.get(), input.data().data(), size);
}

void HandleDataPipeConsumerClose(
    const ::mojolpm::DataPipeConsumerClose& input) {
  mojolpm::GetContext()->RemoveInstance<mojo::ScopedDataPipeConsumerHandle>(
      input.id());
}

void HandleDataPipeProducerClose(
    const ::mojolpm::DataPipeProducerClose& input) {
  mojolpm::GetContext()->RemoveInstance<mojo::ScopedDataPipeProducerHandle>(
      input.id());
}

void HandleSharedBufferRelease(const ::mojolpm::SharedBufferRelease& input) {
  mojolpm::GetContext()->RemoveInstance<mojo::ScopedSharedBufferHandle>(
      input.id());
}
}  // namespace mojolpm
