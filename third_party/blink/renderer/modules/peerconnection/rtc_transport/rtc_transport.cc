// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport.h"

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_config.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/datagram_connection_factory.h"
#include "third_party/webrtc/pc/ice_server_parsing.h"

namespace blink {
namespace {
webrtc::ServerAddresses ParseStunServers(const RtcTransportConfig* config,
                                         ExceptionState& exception_state) {
  webrtc::ServerAddresses stun_servers;
  if (!config->hasIceServers()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing iceServers");
    return stun_servers;
  }

  webrtc::PeerConnectionInterface::IceServers ice_servers;
  for (const RTCIceServer* ice_server : config->iceServers()) {
    if (ice_server->hasUrl()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "'url' field not supported, use 'urls'");
      return stun_servers;
    }
    if (!ice_server->hasUrls()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Ice server must have 'urls'");
      return stun_servers;
    }

    webrtc::PeerConnectionInterface::IceServer server;
    switch (ice_server->urls()->GetContentType()) {
      case V8UnionStringOrStringSequence::ContentType::kString:
        server.urls.push_back(ice_server->urls()->GetAsString().Utf8());
        break;
      case V8UnionStringOrStringSequence::ContentType::kStringSequence:
        for (const String& url : ice_server->urls()->GetAsStringSequence()) {
          server.urls.push_back(url.Utf8());
        }
    }
    ice_servers.push_back(server);
  }
  std::vector<webrtc::RelayServerConfig> unused_turn_servers;
  webrtc::RTCError error = webrtc::ParseIceServersOrError(
      ice_servers, &stun_servers, &unused_turn_servers);
  if (!error.ok()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Failed to parse ice servers");
  }

  return stun_servers;
}

class DatagramConnectionObserver : public webrtc::DatagramConnection::Observer {
 public:
  DatagramConnectionObserver(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      CrossThreadWeakHandle<RtcTransport> transport)
      : main_task_runner_(main_task_runner), transport_(transport) {}
  ~DatagramConnectionObserver() override = default;

  void OnCandidateGathered(const webrtc::Candidate& candidate) override {
    webrtc::Candidate candidate_copy = candidate;
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RtcTransport::OnCandidateGatheredOnMainThread,
                            MakeUnwrappingCrossThreadWeakHandle(transport_),
                            candidate_copy));
  }

  void OnPacketReceived(webrtc::ArrayView<const uint8_t> data) override {
    Vector<uint8_t> data_vec(data);
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RtcTransport::OnPacketReceivedOnMainThread,
                            MakeUnwrappingCrossThreadWeakHandle(transport_),
                            std::move(data_vec)));
  }

  // TODO(crbug.com/443019066): Hook up this with JS events once the API design
  // includes them.
  void OnSendError() override {}
  void OnConnectionError() override {}
  void OnWritableChange() override {}

 private:
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  CrossThreadWeakHandle<RtcTransport> transport_;
};

class AsyncDatagramConnectionImpl : public AsyncDatagramConnection {
 public:
  AsyncDatagramConnectionImpl(
      webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection,
      scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner)
      : datagram_connection_(datagram_connection),
        js_thread_task_runner_(js_thread_task_runner) {}

  ~AsyncDatagramConnectionImpl() override = default;

  void Writable(ScriptPromiseResolver<IDLBoolean>* resolver) override {
    CHECK(js_thread_task_runner_->RunsTasksInCurrentSequence());
    PostCrossThreadTask(
        *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                   datagram_connection,
               CrossThreadHandle<ScriptPromiseResolver<IDLBoolean>> resolver,
               scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner) {
              bool writable = datagram_connection->Writable();

              PostCrossThreadTask(
                  *js_thread_task_runner, FROM_HERE,
                  CrossThreadBindOnce(
                      [](ScriptPromiseResolver<IDLBoolean>* resolver,
                         bool writable) { resolver->Resolve(writable); },
                      MakeUnwrappingCrossThreadHandle(resolver), writable));
            },
            datagram_connection_, MakeCrossThreadHandle(resolver),
            js_thread_task_runner_));
  }

  void Terminate() override {
    // Post the webrtc objects to the network thread for destruction there.
    PostCrossThreadTask(*RtcTransportDependencies::NetworkTaskRunner(),
                        FROM_HERE,
                        CrossThreadBindOnce(
                            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                                   datagram_connection) {
                              datagram_connection->Terminate([] {});
                            },
                            std::move(datagram_connection_)));
  }

  webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection_;
  scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner_;
};

}  // namespace

// static
RtcTransport* RtcTransport::Create(ExecutionContext* context,
                                   const RtcTransportConfig* config,
                                   ExceptionState& exception_state) {
  auto* transport = MakeGarbageCollected<RtcTransport>(PassKey(), context);

  webrtc::ServerAddresses stun_servers =
      ParseStunServers(config, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  RtcTransportDependencies::GetInitialized(
      *context,
      BindOnce(&RtcTransport::ContinueInitialization, WrapPersistent(transport),
               config->iceControlling(), stun_servers));
  return transport;
}

// static
RtcTransport* RtcTransport::CreateForTests(
    ExecutionContext* context,
    const RtcTransportConfig* config,
    ExceptionState& exception_state,
    std::unique_ptr<AsyncDatagramConnection> async_datagram_connection) {
  auto* transport = MakeGarbageCollected<RtcTransport>(PassKey(), context);

  webrtc::ServerAddresses stun_servers =
      ParseStunServers(config, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (async_datagram_connection) {
    transport->OnInitialized(std::move(async_datagram_connection));
  }
  return transport;
}

RtcTransport::RtcTransport(PassKey, ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      task_runner_(context->GetTaskRunner(TaskType::kNetworking)) {
  // Should this be done async? cf
  // RTCCertificateGenerator::GenerateCertificateAsync.
  certificate_ = webrtc::RTCCertificateGenerator::GenerateCertificate(
      webrtc::KeyParams::ECDSA(), /*expires_ms=*/std::nullopt);
}

void RtcTransport::ContinueInitialization(
    bool ice_controlling,
    webrtc::ServerAddresses stun_servers,
    RtcTransportDependencies* dependencies) {
  std::unique_ptr<P2PPortAllocator> port_allocator =
      dependencies->CreatePortAllocator();
  std::vector<webrtc::RelayServerConfig> turn_servers;
  port_allocator->SetConfiguration(stun_servers, turn_servers, 0,
                                   webrtc::PortPrunePolicy::NO_PRUNE);

  auto observer = std::make_unique<DatagramConnectionObserver>(
      task_runner_, MakeCrossThreadWeakHandle(this));
  PostCrossThreadTask(
      *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadHandle<RtcTransport> transport,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             std::unique_ptr<P2PPortAllocator> port_allocator,
             const webrtc::ServerAddresses stun_servers, bool ice_controlling,
             const webrtc::Environment& env,
             webrtc::scoped_refptr<webrtc::RTCCertificate> certificate,
             std::unique_ptr<webrtc::DatagramConnection::Observer> observer) {
            port_allocator->Initialize();

            webrtc::scoped_refptr<webrtc::DatagramConnection>
                datagram_connection = webrtc::CreateDatagramConnection(
                    env, std::move(port_allocator), /*name=*/"RtcTransport",
                    ice_controlling, std::move(certificate),
                    std::move(observer));

            auto async_datagram_connection =
                std::make_unique<AsyncDatagramConnectionImpl>(
                    std::move(datagram_connection), task_runner);
            PostCrossThreadTask(
                *task_runner, FROM_HERE,
                CrossThreadBindOnce(&RtcTransport::OnInitialized,
                                    MakeUnwrappingCrossThreadHandle(transport),
                                    std::move(async_datagram_connection)));
          },
          MakeCrossThreadHandle(this), task_runner_, std::move(port_allocator),
          stun_servers, ice_controlling, dependencies->Environment(),
          certificate_, std::move(observer)));
}

void RtcTransport::OnInitialized(
    std::unique_ptr<AsyncDatagramConnection> async_datagram_connection) {
  DCHECK(!initialized_);
  initialized_ = true;
  async_datagram_connection_ = std::move(async_datagram_connection);
}

RtcTransport::~RtcTransport() = default;

void RtcTransport::OnCandidateGatheredOnMainThread(
    webrtc::Candidate candidate) {
  // TODO(crbug.com/443019066): Fire an appropriate JS event against this.
}

void RtcTransport::OnPacketReceivedOnMainThread(Vector<uint8_t> data) {
  received_packets_.push_back(
      MakeGarbageCollected<RtcReceivedPacket>(DOMArrayBuffer::Create(data)));
}

HeapVector<Member<RtcReceivedPacket>> RtcTransport::getReceivedPackets() {
  HeapVector<Member<RtcReceivedPacket>> packets;
  std::swap(packets, received_packets_);
  return packets;
}

void RtcTransport::sendPackets(
    HeapVector<Member<RtcSendPacketParameters>> packets) {
  // TODO(crbug.com/443019066): Hook up to an actual transport.
  NOTIMPLEMENTED();
}

ScriptPromise<IDLBoolean> RtcTransport::writable(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);

  if (initialized_) {
    async_datagram_connection_->Writable(resolver);
  } else {
    resolver->Resolve(false);
  }

  return resolver->Promise();
}

void RtcTransport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(received_packets_);
}

void RtcTransport::Dispose() {
  if (initialized_) {
    async_datagram_connection_->Terminate();
  }
}

}  // namespace blink
