// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
#define GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/image_decode_accelerator_proxy.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/message_filter.h"
#include "ipc/message_router.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace IPC {
struct PendingSyncMsg;
class ChannelMojo;
}
struct GpuDeferredMessage;

namespace gpu {
struct SyncToken;
class GpuChannelHost;
class GpuMemoryBufferManager;

using GpuChannelEstablishedCallback =
    base::OnceCallback<void(scoped_refptr<GpuChannelHost>)>;

class GPU_EXPORT GpuChannelEstablishFactory {
 public:
  virtual ~GpuChannelEstablishFactory() = default;

  virtual void EstablishGpuChannel(GpuChannelEstablishedCallback callback) = 0;
  virtual scoped_refptr<GpuChannelHost> EstablishGpuChannelSync() = 0;
  virtual GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GPU_EXPORT GpuChannelHost
    : public IPC::Sender,
      public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  GpuChannelHost(int channel_id,
                 const gpu::GPUInfo& gpu_info,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 mojo::ScopedMessagePipeHandle handle);

  bool IsLost() const {
    DCHECK(listener_.get());
    return listener_->IsLost();
  }

  int channel_id() const { return channel_id_; }

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }
  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Enqueue a deferred message for the ordering barrier and return an
  // identifier that can be used to ensure or verify the deferred message later.
  uint32_t OrderingBarrier(int32_t route_id,
                           int32_t put_offset,
                           std::vector<SyncToken> sync_token_fences);

  // Enqueues an IPC message that is deferred until the next implicit or
  // explicit flush. The IPC is also possibly gated on one or more SyncTokens
  // being released, but is handled in-order relative to other such IPCs and/or
  // OrderingBarriers. Returns a deferred message id just like OrderingBarrier.
  uint32_t EnqueueDeferredMessage(
      const IPC::Message& message,
      std::vector<SyncToken> sync_token_fences = {});

  // Ensure that the all deferred messages prior upto |deferred_message_id| have
  // been flushed. Pass UINT32_MAX to force all pending deferred messages to be
  // flushed.
  void EnsureFlush(uint32_t deferred_message_id);

  // Verify that the all deferred messages prior upto |deferred_message_id| have
  // reached the service. Pass UINT32_MAX to force all pending deferred messages
  // to be verified.
  void VerifyFlush(uint32_t deferred_message_id);

  // Destroy this channel. Must be called on the main thread, before
  // destruction.
  void DestroyChannel();

  // Add a message route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);

  // Add a message route to be handled on the provided |task_runner|.
  void AddRouteWithTaskRunner(
      int route_id,
      base::WeakPtr<IPC::Listener> listener,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Remove the message route associated with |route_id|.
  void RemoveRoute(int route_id);

  // Reserve one unused image ID.
  int32_t ReserveImageId();

  // Generate a route ID guaranteed to be unique for this channel.
  int32_t GenerateRouteID();

  // Crashes the GPU process. This functionality is added here because
  // of instability when creating a new tab just to navigate to
  // chrome://gpucrash . This only works when running tests and is
  // otherwise ignored.
  void CrashGpuProcessForTesting();

  SharedImageInterface* shared_image_interface() {
    return &shared_image_interface_;
  }

  ImageDecodeAcceleratorProxy* image_decode_accelerator_proxy() {
    return &image_decode_accelerator_proxy_;
  }

 protected:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  ~GpuChannelHost() override;

 private:
  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class GPU_EXPORT Listener : public IPC::Listener {
   public:
    Listener(mojo::ScopedMessagePipeHandle handle,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

    ~Listener() override;

    // Called on the IO thread.
    void Close();

    // Called on the IO thread.
    void AddRoute(int32_t route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    // Called on the IO thread.
    void RemoveRoute(int32_t route_id);

    // IPC::Listener implementation
    // (called on the IO thread):
    bool OnMessageReceived(const IPC::Message& msg) override;
    void OnChannelError() override;

    void SendMessage(std::unique_ptr<IPC::Message> msg,
                     IPC::PendingSyncMsg* pending_sync);

    // The following methods can be called on any thread.

    // Whether the channel is lost.
    bool IsLost() const;

   private:
    struct RouteInfo {
      RouteInfo();
      RouteInfo(const RouteInfo& other);
      RouteInfo(RouteInfo&& other);
      ~RouteInfo();
      RouteInfo& operator=(const RouteInfo& other);
      RouteInfo& operator=(RouteInfo&& other);

      base::WeakPtr<IPC::Listener> listener;
      scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    };

    // Threading notes: most fields are only accessed on the IO thread, except
    // for lost_ which is protected by |lock_|.
    std::unordered_map<int32_t, RouteInfo> routes_;
    std::unique_ptr<IPC::ChannelMojo> channel_;
    base::flat_map<int, IPC::PendingSyncMsg*> pending_syncs_;

    // Protects all fields below this one.
    mutable base::Lock lock_;

    // Whether the channel has been lost.
    bool lost_ = false;
  };

  struct OrderingBarrierInfo {
    OrderingBarrierInfo();
    ~OrderingBarrierInfo();
    OrderingBarrierInfo(OrderingBarrierInfo&&);
    OrderingBarrierInfo& operator=(OrderingBarrierInfo&&);

    // Route ID of the command buffer for this command buffer flush.
    int32_t route_id;
    // Client put offset. Service get offset is updated in shared memory.
    int32_t put_offset;
    // Increasing counter for the deferred message.
    uint32_t deferred_message_id;
    // Sync token dependencies of the message. These are sync tokens for which
    // waits are in the commands that are part of this command buffer flush.
    std::vector<SyncToken> sync_token_fences;
  };

  void EnqueuePendingOrderingBarrier();
  void InternalFlush(uint32_t deferred_message_id);

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_image_id_|, atomic type
  // - |next_route_id_|, atomic type
  // - |deferred_messages_| and |*_deferred_message_id_| protected by
  // |context_lock_|
  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  const int channel_id_;
  const gpu::GPUInfo gpu_info_;
  const gpu::GpuFeatureInfo gpu_feature_info_;

  // Lifetime/threading notes: Listener only operates on the IO thread, and
  // outlives |this|. It is therefore safe to PostTask calls to the IO thread
  // with base::Unretained(listener_).
  std::unique_ptr<Listener, base::OnTaskRunnerDeleter> listener_;

  SharedImageInterfaceProxy shared_image_interface_;

  // A client-side helper to send image decode requests to the GPU process.
  ImageDecodeAcceleratorProxy image_decode_accelerator_proxy_;

  // Image IDs are allocated in sequence.
  base::AtomicSequenceNumber next_image_id_;

  // Route IDs are allocated in sequence.
  base::AtomicSequenceNumber next_route_id_;

  // Protects |deferred_messages_|, |pending_ordering_barrier_| and
  // |*_deferred_message_id_|.
  mutable base::Lock context_lock_;
  std::vector<GpuDeferredMessage> deferred_messages_;
  base::Optional<OrderingBarrierInfo> pending_ordering_barrier_;
  uint32_t next_deferred_message_id_ = 1;
  // Highest deferred message id in |deferred_messages_|.
  uint32_t enqueued_deferred_message_id_ = 0;
  // Highest deferred message id sent to the channel.
  uint32_t flushed_deferred_message_id_ = 0;
  // Highest deferred message id known to have been received by the service.
  uint32_t verified_deferred_message_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
