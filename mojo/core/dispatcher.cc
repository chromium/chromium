// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/dispatcher.h"

#include "base/logging.h"
#include "mojo/core/configuration.h"
#include "mojo/core/data_pipe_consumer_dispatcher.h"
#include "mojo/core/data_pipe_producer_dispatcher.h"
#include "mojo/core/message_pipe_dispatcher.h"
#include "mojo/core/platform_handle_dispatcher.h"
#include "mojo/core/ports/event.h"
#include "mojo/core/shared_buffer_dispatcher.h"

namespace mojo {
namespace core {

namespace {

constinit thread_local bool is_extracting_handles_from_message = false;

}  // namespace

Dispatcher::DispatcherInTransit::DispatcherInTransit() = default;

Dispatcher::DispatcherInTransit::DispatcherInTransit(
    const DispatcherInTransit& other) = default;

Dispatcher::DispatcherInTransit::~DispatcherInTransit() = default;

// static
void Dispatcher::SetExtractingHandlesFromMessage(bool extracting) {
  is_extracting_handles_from_message = extracting;
}

// static
void Dispatcher::AssertNotExtractingHandlesFromMessage() {
  DCHECK(!is_extracting_handles_from_message);
}

MojoResult Dispatcher::WatchDispatcher(scoped_refptr<Dispatcher> dispatcher,
                                       MojoHandleSignals signals,
                                       MojoTriggerCondition condition,
                                       uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::CancelWatch(uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::Arm(uint32_t* num_blocking_events,
                           MojoTrapEvent* blocking_events) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteMessage(
    std::unique_ptr<ports::UserMessageEvent> message) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadMessage(
    std::unique_ptr<ports::UserMessageEvent>* message) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::DuplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::MapBuffer(
    uint64_t offset,
    uint64_t num_bytes,
    std::unique_ptr<PlatformSharedMemoryMapping>* mapping) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::GetBufferInfo(MojoSharedBufferInfo* info) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadData(const MojoReadDataOptions& options,
                                void* elements,
                                uint32_t* num_bytes) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginReadData(const void** buffer,
                                     uint32_t* buffer_num_bytes) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndReadData(uint32_t num_bytes_read) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteData(const void* elements,
                                 uint32_t* num_bytes,
                                 const MojoWriteDataOptions& options) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginWriteData(void** buffer,
                                      uint32_t* buffer_num_bytes,
                                      MojoBeginWriteDataFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndWriteData(uint32_t num_bytes_written) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::AttachMessagePipe(std::string_view name,
                                         ports::PortRef remote_peer_port) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ExtractMessagePipe(std::string_view name,
                                          MojoHandle* message_pipe_handle) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::SetQuota(MojoQuotaType type, uint64_t limit) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::QueryQuota(MojoQuotaType type,
                                  uint64_t* limit,
                                  uint64_t* usage) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

HandleSignalsState Dispatcher::GetHandleSignalsState() const {
  return HandleSignalsState();
}

MojoResult Dispatcher::AddWatcherRef(
    const scoped_refptr<WatcherDispatcher>& watcher,
    uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::RemoveWatcherRef(WatcherDispatcher* watcher,
                                        uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

void Dispatcher::StartSerialize(uint32_t* num_bytes,
                                uint32_t* num_ports,
                                uint32_t* num_platform_handles) {
  *num_bytes = 0;
  *num_ports = 0;
  *num_platform_handles = 0;
}

bool Dispatcher::EndSerialize(void* destination,
                              ports::PortName* ports,
                              PlatformHandle* handles) {
  LOG(ERROR) << "Attempting to serialize a non-transferrable dispatcher.";
  return true;
}

bool Dispatcher::BeginTransit() {
  return true;
}

void Dispatcher::CompleteTransitAndClose() {}

void Dispatcher::CancelTransit() {}

// static
scoped_refptr<Dispatcher> Dispatcher::Deserialize(
    Type type,
    const void* bytes,
    size_t num_bytes,
    const ports::PortName* ports,
    size_t num_ports,
    PlatformHandle* platform_handles,
    size_t num_platform_handles) {
  switch (type) {
    case Type::MESSAGE_PIPE:
      return MessagePipeDispatcher::Deserialize(bytes, num_bytes, ports,
                                                num_ports, platform_handles,
                                                num_platform_handles);
    case Type::SHARED_BUFFER:
      return SharedBufferDispatcher::Deserialize(bytes, num_bytes, ports,
                                                 num_ports, platform_handles,
                                                 num_platform_handles);
    case Type::DATA_PIPE_CONSUMER:
      return DataPipeConsumerDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::DATA_PIPE_PRODUCER:
      return DataPipeProducerDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::PLATFORM_HANDLE:
      return PlatformHandleDispatcher::Deserialize(bytes, num_bytes, ports,
                                                   num_ports, platform_handles,
                                                   num_platform_handles);
    default:
      LOG(ERROR) << "Deserializing invalid dispatcher type.";
      return nullptr;
  }
}

Dispatcher::Dispatcher() = default;

Dispatcher::~Dispatcher() = default;

}  // namespace core
}  // namespace mojo
