/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Network.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace network {
namespace mojom {
namespace blink {
class WebSocketHandshakeResponse;
class WebSocketHandshakeRequest;
}  // namespace blink
}  // namespace mojom
}  // namespace network

namespace blink {

class BlobDataHandle;
class Document;
class DocumentLoader;
class ExecutionContext;
struct FetchInitiatorInfo;
class LocalFrame;
class HTTPHeaderMap;
class KURL;
class NetworkResourcesData;
class Resource;
class ResourceError;
class ResourceResponse;
class ThreadableLoaderClient;
class XHRReplayData;
class XMLHttpRequest;
class WorkerGlobalScope;
enum class ResourceRequestBlockedReason;
enum class ResourceType : uint8_t;

class CORE_EXPORT InspectorNetworkAgent final
    : public InspectorBaseAgent<protocol::Network::Metainfo> {
 public:
  // TODO(horo): Extract the logic for frames and for workers into different
  // classes.
  InspectorNetworkAgent(InspectedFrames*,
                        WorkerGlobalScope*,
                        v8_inspector::V8InspectorSession*);
  ~InspectorNetworkAgent() override;
  void Trace(blink::Visitor*) override;

  void Restore() override;

  // Probes.
  void DidBlockRequest(ExecutionContext*,
                       const ResourceRequest&,
                       DocumentLoader*,
                       const FetchInitiatorInfo&,
                       ResourceRequestBlockedReason,
                       ResourceType);
  void DidChangeResourcePriority(DocumentLoader*,
                                 unsigned long identifier,
                                 ResourceLoadPriority);
  void WillSendRequest(ExecutionContext*,
                       unsigned long identifier,
                       DocumentLoader*,
                       ResourceRequest&,
                       const ResourceResponse& redirect_response,
                       const FetchInitiatorInfo&,
                       ResourceType);
  void MarkResourceAsCached(DocumentLoader*, unsigned long identifier);
  void DidReceiveResourceResponse(unsigned long identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  Resource*);
  void DidReceiveData(unsigned long identifier,
                      DocumentLoader*,
                      const char* data,
                      int data_length);
  void DidReceiveBlob(unsigned long identifier,
                      DocumentLoader*,
                      scoped_refptr<BlobDataHandle>);
  void DidReceiveEncodedDataLength(DocumentLoader*,
                                   unsigned long identifier,
                                   int encoded_data_length);
  void DidFinishLoading(unsigned long identifier,
                        DocumentLoader*,
                        TimeTicks monotonic_finish_time,
                        int64_t encoded_data_length,
                        int64_t decoded_body_length,
                        bool should_report_corb_blocking);
  void DidReceiveCORSRedirectResponse(unsigned long identifier,
                                      DocumentLoader*,
                                      const ResourceResponse&,
                                      Resource*);
  void DidFailLoading(unsigned long identifier,
                      DocumentLoader*,
                      const ResourceError&);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void ScriptImported(unsigned long identifier, const String& source_string);
  void DidReceiveScriptResponse(unsigned long identifier);
  void ShouldForceCORSPreflight(bool* result);
  void ShouldBlockRequest(const KURL&, bool* result);
  void ShouldBypassServiceWorker(bool* result);

  void WillLoadXHR(XMLHttpRequest*,
                   ThreadableLoaderClient*,
                   const AtomicString& method,
                   const KURL&,
                   bool async,
                   const HTTPHeaderMap& headers,
                   bool include_crendentials);
  void DidFinishXHR(XMLHttpRequest*);

  void WillSendEventSourceRequest(ThreadableLoaderClient*);
  void WillDispatchEventSourceEvent(unsigned long identifier,
                                    const AtomicString& event_name,
                                    const AtomicString& event_id,
                                    const String& data);

  void WillDestroyResource(Resource*);

  void FrameScheduledNavigation(LocalFrame*, ScheduledNavigation*);
  void FrameClearedScheduledNavigation(LocalFrame*);

  void DidCreateWebSocket(ExecutionContext*,
                          unsigned long identifier,
                          const KURL& request_url,
                          const String&);
  void WillSendWebSocketHandshakeRequest(
      ExecutionContext*,
      unsigned long identifier,
      network::mojom::blink::WebSocketHandshakeRequest*);
  void DidReceiveWebSocketHandshakeResponse(
      ExecutionContext*,
      unsigned long identifier,
      network::mojom::blink::WebSocketHandshakeRequest*,
      network::mojom::blink::WebSocketHandshakeResponse*);
  void DidCloseWebSocket(ExecutionContext*, unsigned long identifier);
  void DidReceiveWebSocketFrame(unsigned long identifier,
                                int op_code,
                                bool masked,
                                const char* payload,
                                size_t payload_length);
  void DidSendWebSocketFrame(unsigned long identifier,
                             int op_code,
                             bool masked,
                             const char* payload,
                             size_t payload_length);
  void DidReceiveWebSocketFrameError(unsigned long identifier, const String&);

  // Called from frontend
  protocol::Response enable(Maybe<int> total_buffer_size,
                            Maybe<int> resource_buffer_size,
                            Maybe<int> max_post_data_size) override;
  protocol::Response disable() override;
  protocol::Response setExtraHTTPHeaders(
      std::unique_ptr<protocol::Network::Headers>) override;
  void getResponseBody(const String& request_id,
                       std::unique_ptr<GetResponseBodyCallback>) override;
  protocol::Response searchInResponseBody(
      const String& request_id,
      const String& query,
      Maybe<bool> case_sensitive,
      Maybe<bool> is_regex,
      std::unique_ptr<
          protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>*
          matches) override;

  protocol::Response setBlockedURLs(
      std::unique_ptr<protocol::Array<String>> urls) override;
  protocol::Response replayXHR(const String& request_id) override;
  protocol::Response canClearBrowserCache(bool* result) override;
  protocol::Response canClearBrowserCookies(bool* result) override;
  protocol::Response emulateNetworkConditions(
      bool offline,
      double latency,
      double download_throughput,
      double upload_throughput,
      Maybe<String> connection_type) override;
  protocol::Response setCacheDisabled(bool) override;
  protocol::Response setBypassServiceWorker(bool) override;
  protocol::Response setDataSizeLimitsForTest(int max_total_size,
                                              int max_resource_size) override;
  protocol::Response getCertificate(
      const String& origin,
      std::unique_ptr<protocol::Array<String>>* certificate) override;

  void getRequestPostData(const String& request_id,
                          std::unique_ptr<GetRequestPostDataCallback>) override;

  // Called from other agents.
  protocol::Response GetResponseBody(const String& request_id,
                                     String* content,
                                     bool* base64_encoded);
  bool FetchResourceContent(Document*,
                            const KURL&,
                            String* content,
                            bool* base64_encoded);
  String NavigationInitiatorInfo(LocalFrame*);

 private:
  void Enable();
  void WillSendRequestInternal(ExecutionContext*,
                               unsigned long identifier,
                               DocumentLoader*,
                               const ResourceRequest&,
                               const ResourceResponse& redirect_response,
                               const FetchInitiatorInfo&,
                               InspectorPageAgent::ResourceType);
  void RemoveFinishedReplayXHRFired(TimerBase*);
  void DidFinishXHRInternal(ExecutionContext*,
                            XMLHttpRequest*,
                            ThreadableLoaderClient*,
                            const AtomicString&,
                            const String&,
                            bool);

  bool CanGetResponseBodyBlob(const String& request_id);
  void GetResponseBodyBlob(const String& request_id,
                           std::unique_ptr<GetResponseBodyCallback>);

  static std::unique_ptr<protocol::Network::Initiator> BuildInitiatorObject(
      Document*,
      const FetchInitiatorInfo&);
  static bool IsNavigation(DocumentLoader*, unsigned long identifier);

  // This is null while inspecting workers.
  Member<InspectedFrames> inspected_frames_;
  // This is null while inspecting frames.
  Member<WorkerGlobalScope> worker_global_scope_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<NetworkResourcesData> resources_data_;
  const base::UnguessableToken devtools_token_;

  // Stores the pending ThreadableLoaderClient till an identifier for
  // the load is generated by the loader and passed to the inspector
  // via the WillSendRequest() method.
  ThreadableLoaderClient* pending_request_;
  InspectorPageAgent::ResourceType pending_request_type_;

  Member<XHRReplayData> pending_xhr_replay_data_;

  HashMap<String, std::unique_ptr<protocol::Network::Initiator>>
      frame_navigation_initiator_map_;

  HeapHashSet<Member<XMLHttpRequest>> replay_xhrs_;
  HeapHashSet<Member<XMLHttpRequest>> replay_xhrs_to_be_deleted_;
  TaskRunnerTimer<InspectorNetworkAgent> remove_finished_replay_xhr_timer_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Boolean cache_disabled_;
  InspectorAgentState::Boolean bypass_service_worker_;
  InspectorAgentState::BooleanMap blocked_urls_;
  InspectorAgentState::StringMap extra_request_headers_;
  InspectorAgentState::Integer total_buffer_size_;
  InspectorAgentState::Integer resource_buffer_size_;
  InspectorAgentState::Integer max_post_data_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_
