// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_CORE_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/ports/port_ref.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/core/watcher_set.h"
#include "mojo/public/c/system/data_pipe.h"

namespace mojo {
namespace core {

class NodeController;

// This is the Dispatcher implementation for the consumer handle for data
// pipes created by the Mojo primitive MojoCreateDataPipe(). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher final
    : public Dispatcher {
 public:
  static scoped_refptr<DataPipeConsumerDispatcher> Create(
      NodeController* node_controller,
      const ports::PortRef& control_port,
      base::UnsafeSharedMemoryRegion shared_ring_buffer,
      const MojoCreateDataPipeOptions& options,
      uint64_t pipe_id);

  DataPipeConsumerDispatcher(const DataPipeConsumerDispatcher&) = delete;
  DataPipeConsumerDispatcher& operator=(const DataPipeConsumerDispatcher&) =
      delete;

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult ReadData(const MojoReadDataOptions& validated_options,
                      void* elements,
                      uint32_t* num_bytes) override;
  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes) override;
  MojoResult EndReadData(uint32_t num_bytes_read) override;
  HandleSignalsState GetHandleSignalsState() const override;
  MojoResult AddWatcherRef(const scoped_refptr<WatcherDispatcher>& watcher,
                           uintptr_t context) override;
  MojoResult RemoveWatcherRef(WatcherDispatcher* watcher,
                              uintptr_t context) override;
  void StartSerialize(uint32_t* num_bytes,
                      uint32_t* num_ports,
                      uint32_t* num_handles) override;
  bool EndSerialize(void* destination,
                    ports::PortName* ports,
                    PlatformHandle* handles) override;
  bool BeginTransit() override;
  void CompleteTransitAndClose() override;
  void CancelTransit() override;

  static scoped_refptr<DataPipeConsumerDispatcher> Deserialize(
      const void* data,
      size_t num_bytes,
      const ports::PortName* ports,
      size_t num_ports,
      PlatformHandle* handles,
      size_t num_handles);

 private:
  class PortObserverThunk;
  friend class PortObserverThunk;

  DataPipeConsumerDispatcher(NodeController* node_controller,
                             const ports::PortRef& control_port,
                             base::UnsafeSharedMemoryRegion shared_ring_buffer,
                             const MojoCreateDataPipeOptions& options,
                             uint64_t pipe_id);
  ~DataPipeConsumerDispatcher() override;

  bool InitializeNoLock();
  MojoResult CloseNoLock();
  HandleSignalsState GetHandleSignalsStateNoLock() const;
  void NotifyRead(uint32_t num_bytes);
  void OnPortStatusChanged();
  void UpdateSignalsStateNoLock();

  const MojoCreateDataPipeOptions options_;
  const raw_ptr<NodeController> node_controller_;
  const ports::PortRef control_port_;
  const uint64_t pipe_id_;

  // Guards access to the fields below.
  mutable base::Lock lock_;

  WatcherSet watchers_;

  base::UnsafeSharedMemoryRegion shared_ring_buffer_;

  // We don't really write to it, and it's safe because we're the only consumer
  // of this buffer.
  base::WritableSharedMemoryMapping ring_buffer_mapping_;

  bool in_two_phase_read_ = false;
  uint32_t two_phase_max_bytes_read_ = 0;

  bool in_transit_ = false;
  bool is_closed_ = false;
  bool peer_closed_ = false;
  bool peer_remote_ = false;
  bool transferred_ = false;

  uint32_t read_offset_ = 0;
  uint32_t bytes_available_ = 0;

  // Indicates whether any new data is available since the last read attempt.
  bool new_data_available_ = false;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_DATA_PIPE_CONSUMER_DISPATCHER_H_
