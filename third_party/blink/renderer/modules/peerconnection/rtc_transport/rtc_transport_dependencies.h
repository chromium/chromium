// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_DEPENDENCIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_DEPENDENCIES_H_

#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/p2p/port_allocator.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class P2PSocketDispatcher;

class MODULES_EXPORT RtcTransportDependencies
    : public GarbageCollected<RtcTransportDependencies>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleObserver {
 public:
  static const char kSupplementName[];

  static scoped_refptr<base::SingleThreadTaskRunner> NetworkTaskRunner();
  // Gets from, invoking callback once initialized and ready.
  static void GetInitialized(
      ExecutionContext& context,
      base::OnceCallback<void(RtcTransportDependencies*)> callback);

  RtcTransportDependencies(ExecutionContext& context,
                           base::PassKey<RtcTransportDependencies>);

  RtcTransportDependencies(const RtcTransportDependencies&) = delete;
  RtcTransportDependencies& operator=(const RtcTransportDependencies&) = delete;

  ~RtcTransportDependencies() override;

  std::unique_ptr<P2PPortAllocator> CreatePortAllocator();

  webrtc::Environment& Environment() { return webrtc_environment_; }

  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  void OnInitialized(std::unique_ptr<IpcNetworkManager> network_manager,
                     std::unique_ptr<IpcPacketSocketFactory> socket_factory);
  // Whether async initialization has finished.
  bool initialized_ = false;
  Vector<base::OnceClosure> initialized_callback_list_;
  void RunOnceInitialized(base::OnceClosure initialized_callback);

  Member<P2PSocketDispatcher> p2p_socket_dispatcher_;
  std::unique_ptr<IpcNetworkManager> network_manager_;
  std::unique_ptr<IpcPacketSocketFactory> socket_factory_;

  webrtc::Environment webrtc_environment_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_DEPENDENCIES_H_
