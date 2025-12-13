// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"

#include "base/threading/thread.h"
#include "components/webrtc/thread_wrapper.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/p2p/filtering_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"
#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/environment/environment_factory.h"

namespace blink {
namespace {
// Compare to the traffic annotation tag in PeerConnectionDependencyFactory.
constexpr net::NetworkTrafficAnnotationTag kRtcTransportTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webrtc_rtc_transport", R"(
        semantics {
          sender: "WebRTC RtcTransport"
          description:
            "WebRTC RtcTransport is an API providing web applications with "
            "Real Time Communication (RTC) capabilities. It is used to establish a "
            "secure session with a remote peer, transmitting and receiving "
            "audio, video and potentially other data."
          trigger:
            "Application creates an RtcTransport object and connects it to a "
            "remote peer by exchanging ICE candidates and DTLS negotiation."
          user_data: {
            type: WEB_CONTENT
          }
          data:
            "Media encrypted using DTLS or DTLS-SRTP, and protocol-level messages for "
            "the various subprotocols employed by WebRTC (including ICE, DTLS, "
            "RTCP, etc.). Note that ICE connectivity checks may leak the "
            "user's IP address(es), subject to the restrictions/guidance in "
            "https://datatracker.ietf.org/doc/draft-ietf-rtcweb-ip-handling."
          destination: OTHER
          destination_other:
            "A destination determined by the web application that created the "
            "connection."
          internal: {
            contacts: {
              owners: "third_party/blink/renderer/modules/peerconnection/OWNERS"
            }
          }
          last_reviewed: "2025-09-19"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it won't be used "
            "unless the application creates an RtcTransport. Media can "
            "only be captured with user's consent, but data may be sent "
            "without that."
          policy_exception_justification:
            "Not implemented. 'WebRtcUdpPortRange' policy can limit the range "
            "of ports used by WebRTC, but there is no policy to generally "
            "block it."
        }
    )");

}  // namespace

using PassKey = base::PassKey<RtcTransportDependencies>;

class RtcTransportProcessWideDeps {
 public:
  RtcTransportProcessWideDeps() : network_thread_("RtcTransport_network") {
    network_thread_.StartWithOptions(
        base::Thread::Options(base::ThreadType::kDefault));

    base::WaitableEvent network_thread_wrapped;
    PostCrossThreadTask(
        *network_thread_.task_runner(), FROM_HERE,
        CrossThreadBindOnce(
            [](base::WaitableEvent* event) {
              webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
              webrtc::ThreadWrapper::current()->set_send_allowed(true);
              event->Signal();
            },
            CrossThreadUnretained(&network_thread_wrapped)));
    network_thread_wrapped.Wait();
  }

  ~RtcTransportProcessWideDeps() = default;

  base::Thread& GetNetworkThread() { return network_thread_; }

 private:
  base::Thread network_thread_;
};

RtcTransportProcessWideDeps& ProcessWideDeps() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(RtcTransportProcessWideDeps, instance, ());
  return instance;
}

// static
const char RtcTransportDependencies::kSupplementName[] =
    "RtcTransportDependencies";

// static
scoped_refptr<base::SingleThreadTaskRunner>
RtcTransportDependencies::NetworkTaskRunner() {
  return ProcessWideDeps().GetNetworkThread().task_runner();
}

// static
void RtcTransportDependencies::GetInitialized(
    ExecutionContext& context,
    base::OnceCallback<void(RtcTransportDependencies*)> callback) {
  CHECK(!context.IsContextDestroyed());
  RtcTransportDependencies* supplement =
      Supplement<ExecutionContext>::From<RtcTransportDependencies>(context);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<RtcTransportDependencies>(context, PassKey());
    ProvideTo(context, supplement);
  }
  supplement->RunOnceInitialized(
      base::BindOnce(std::move(callback), WrapPersistent(supplement)));
}

RtcTransportDependencies::RtcTransportDependencies(ExecutionContext& context,
                                                   PassKey)
    : Supplement(context),
      ExecutionContextLifecycleObserver(&context),
      p2p_socket_dispatcher_(P2PSocketDispatcher::From(context)),
      webrtc_environment_(webrtc::EnvironmentFactory().Create()) {
  // Start initialization on the network thread.
  PostCrossThreadTask(
      *NetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadHandle<RtcTransportDependencies>
                 rtc_transport_dependencies,
             P2PSocketDispatcher* p2p_socket_dispatcher,
             scoped_refptr<base::SequencedTaskRunner> task_runner) {
            std::unique_ptr<IpcNetworkManager> network_manager =
                std::make_unique<IpcNetworkManager>(p2p_socket_dispatcher,
                                                    /*mdns_responder=*/nullptr);
            auto devtools_token_getter =
                [](base::OnceCallback<void(
                       std::optional<base::UnguessableToken>)> then) {
                  // TODO(crbug.com/443019066): Actually get a devtools token.
                  std::move(then).Run(std::nullopt);
                };
            std::unique_ptr<IpcPacketSocketFactory> socket_factory =
                std::make_unique<IpcPacketSocketFactory>(
                    CrossThreadBindRepeating(devtools_token_getter),
                    p2p_socket_dispatcher, kRtcTransportTrafficAnnotation,
                    /*batch_udp_packets=*/true);

            PostCrossThreadTask(
                *task_runner, FROM_HERE,
                CrossThreadBindOnce(
                    &RtcTransportDependencies::OnInitialized,
                    MakeUnwrappingCrossThreadHandle(rtc_transport_dependencies),
                    std::move(network_manager), std::move(socket_factory)));
          },
          MakeCrossThreadHandle(this),
          CrossThreadPersistent(p2p_socket_dispatcher_.Get()),
          context.GetTaskRunner(TaskType::kNetworking)));
}

RtcTransportDependencies::~RtcTransportDependencies() {
  PostCrossThreadTask(
      *NetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<IpcNetworkManager> network_manager,
             std::unique_ptr<IpcPacketSocketFactory> socket_factory) {
            network_manager.reset();
            socket_factory.reset();
          },
          std::move(network_manager_), std::move(socket_factory_)));
}

void RtcTransportDependencies::RunOnceInitialized(
    base::OnceClosure initialized_callback) {
  if (initialized_) {
    std::move(initialized_callback).Run();
  } else {
    initialized_callback_list_.push_back(std::move(initialized_callback));
  }
}

void RtcTransportDependencies::OnInitialized(
    std::unique_ptr<IpcNetworkManager> network_manager,
    std::unique_ptr<IpcPacketSocketFactory> socket_factory) {
  initialized_ = true;
  network_manager_ = std::move(network_manager);
  socket_factory_ = std::move(socket_factory);

  for (auto& callback : initialized_callback_list_) {
    std::move(callback).Run();
  }
}

std::unique_ptr<P2PPortAllocator>
RtcTransportDependencies::CreatePortAllocator() {
  CHECK(initialized_);
  auto network_manager = std::make_unique<FilteringNetworkManager>(
      network_manager_.get(), /*media_permission=*/nullptr,
      /*allow_mdns_obfuscation=*/true);

  auto port_allocator = std::make_unique<P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(),
      P2PPortAllocator::Config(),
      /*lna_permission_factory=*/nullptr);

  int port_allocator_flags = port_allocator->flags();
  port_allocator_flags |= webrtc::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                          webrtc::PORTALLOCATOR_ENABLE_IPV6 |
                          webrtc::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI |
                          webrtc::PORTALLOCATOR_DISABLE_TCP;
  port_allocator->set_flags(port_allocator_flags);

  return port_allocator;
}

void RtcTransportDependencies::ContextDestroyed() {
  if (network_manager_) {
    PostCrossThreadTask(
        *NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&IpcNetworkManager::ContextDestroyed,
                            CrossThreadUnretained(network_manager_.get())));
  }
}

void RtcTransportDependencies::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(p2p_socket_dispatcher_);
}

}  // namespace blink
