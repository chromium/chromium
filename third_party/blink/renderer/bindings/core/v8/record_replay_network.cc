// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/record_replay_network.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "v8/include/v8.h"

using namespace blink;

namespace recordreplay {

static bool PermitRecordReplayBrowserEvents() {
  return IsRecordingOrReplaying("notify-network") && v8::IsMainThread();
}

static const char* HttpVersionToString(blink::ResourceResponse::HTTPVersion version) {
  switch (version) {
    case blink::ResourceResponse::HTTPVersion::kHTTPVersion_0_9:
      return "http/0.9";
    case blink::ResourceResponse::HTTPVersion::kHTTPVersion_1_0:
      return "http/1.0";
    case blink::ResourceResponse::HTTPVersion::kHTTPVersion_1_1:
      return "http/1.1";
    case blink::ResourceResponse::HTTPVersion::kHTTPVersion_2_0:
      return "http/2.0";
    case blink::ResourceResponse::HTTPVersion::kHTTPVersionUnknown:
      return "http";
    default:
      return "http/unknown";
  }
}

static std::string RecordReplayNetworkRequestId(uint64_t inspector_id) {
  // Inspector identifiers can vary when replaying due to differences in inspector
  // behavior. Make sure the identifiers we report to the recorder are consistent
  // by manually recording/replaying the identifier.
  uint64_t identifier = RecordReplayValue("NetworkRequestId", inspector_id);

  char request_id[64];
  snprintf(request_id, 64, "%d.%lu", (int) base::GetCurrentProcId(), (unsigned long) identifier);
  return std::string(request_id);
}

static const char* GetRequestCauseString(const ResourceRequest& req) {
  switch (req.GetRequestContext()) {
    case mojom::blink::RequestContextType::SCRIPT:
      return "script";
    case mojom::blink::RequestContextType::STYLE:
      return "stylesheet";
    case mojom::blink::RequestContextType::OBJECT:
      return "object";
    case mojom::blink::RequestContextType::IMAGE:
      return "img";
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return "xhr";
    case mojom::blink::RequestContextType::BEACON:
      return "beacon";
    case mojom::blink::RequestContextType::FETCH:
      return "fetch";
    case mojom::blink::RequestContextType::XSLT:
      return "xslt";
    case mojom::blink::RequestContextType::MANIFEST:
      return "webManifest";
    case mojom::blink::RequestContextType::FONT:
      return "font";
    case mojom::blink::RequestContextType::PING:
      return "ping";
    case mojom::blink::RequestContextType::IMAGE_SET:
      return "imageset";
    case mojom::blink::RequestContextType::CSP_REPORT:
      return "csp";
    // ReplayIO/Kannan
    // Remaining are guesses, not quite sure if they're correct (kv).
    case mojom::blink::RequestContextType::IFRAME:
    case mojom::blink::RequestContextType::FRAME:
      return "subdocument";
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::PREFETCH:
      return "document";
    case mojom::blink::RequestContextType::SUBRESOURCE:
      return "subdocument";
    case mojom::blink::RequestContextType::VIDEO:
      return "media";
    // ReplayIO/Kannan
    // The following doesn't have an equivalent in gecko-dev
    case mojom::blink::RequestContextType::FAVICON:
      return "favicon";
    case mojom::blink::RequestContextType::AUDIO:
      return "audio";
    case mojom::blink::RequestContextType::DOWNLOAD:
      return "download";
    case mojom::blink::RequestContextType::EMBED:
      return "embed";
    case mojom::blink::RequestContextType::EVENT_SOURCE:
      return "eventSource";
    case mojom::blink::RequestContextType::FORM:
      return "form";
    case mojom::blink::RequestContextType::INTERNAL:
      return "internal";
    case mojom::blink::RequestContextType::LOCATION:
      return "location";
    case mojom::blink::RequestContextType::PLUGIN:
      return "plugin";
    case mojom::blink::RequestContextType::SERVICE_WORKER:
      return "serviceWorker";
    case mojom::blink::RequestContextType::SHARED_WORKER:
      return "sharedWorker";
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return "subresourceWebbundle";
    case mojom::blink::RequestContextType::TRACK:
      return "track";
    case mojom::blink::RequestContextType::WORKER:
      return "worker";
    case mojom::blink::RequestContextType::UNSPECIFIED:
    default:
      return nullptr;
  }
  /* ReplayIO/Kannan
   * No mappings yet for the following gecko content policy types:
   * [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "objectSubdoc",
   * [Ci.nsIContentPolicy.TYPE_DTD]: "dtd",
   * [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "websocket",
   */
}

static absl::optional<base::DictionaryValue>
BuildInitiatorObject(const blink::Document* document,
                     const blink::FetchInitiatorInfo& initiator_info) {
  // See InspectorNetworkAgent::BuildInitiatorObject for the basis of this
  // function. Note that it would be better if we listened to CDP Network events
  // while replaying so we don't need this logic duplication.

  if (initiator_info.is_imported_module && !initiator_info.referrer.empty()) {
    base::DictionaryValue rv;
    rv.SetString("url", initiator_info.referrer.Utf8());
    rv.SetInteger("line", initiator_info.position.line_.OneBasedInt());
    rv.SetInteger("column", initiator_info.position.column_.ZeroBasedInt());
    return rv;
  }

  bool was_requested_by_stylesheet =
      initiator_info.name == blink::fetch_initiator_type_names::kCSS ||
      initiator_info.name == blink::fetch_initiator_type_names::kUacss;
  if (was_requested_by_stylesheet && !initiator_info.referrer.empty()) {
    base::DictionaryValue rv;
    rv.SetString("url", initiator_info.referrer.Utf8());
    return rv;
  }

  while (document && !document->GetScriptableDocumentParser())
    document = document->LocalOwner() ? document->LocalOwner()->ownerDocument()
                                      : nullptr;
  if (document && document->GetScriptableDocumentParser()) {
    base::DictionaryValue rv;

    blink::KURL url = document->Url();
    url.RemoveFragmentIdentifier();
    rv.SetString("url", url.GetString().Utf8());

    if (TextPosition::BelowRangePosition() != initiator_info.position) {
      rv.SetInteger("line", initiator_info.position.line_.OneBasedInt());
      rv.SetInteger("column", initiator_info.position.column_.ZeroBasedInt());
    } else {
      rv.SetInteger("line", document->GetScriptableDocumentParser()->GetTextPosition().line_.OneBasedInt());
      rv.SetInteger("column", document->GetScriptableDocumentParser()->GetTextPosition().column_.ZeroBasedInt());
    }

    return rv;
  }

  return absl::optional<base::DictionaryValue>();
}

void OnNetworkPrepareRequest(const blink::Document* document, const blink::Resource* resource,
                             const blink::ResourceRequest& request) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  // We must allow user agent scripts when taking a new bookmark.
  blink::ScriptForbiddenScope::AllowUserAgentScript allow_script;
  std::string url_string = request.Url().GetString().Utf8().c_str();

  // Capture the record replay bookmark for the network request here,
  // where the devtools stack id is taken.
  uint64_t bookmark = NewBookmark();

  std::string requestId = RecordReplayNetworkRequestId(request.InspectorId());

  if (recordreplay::DependencyGraphEnabled()) {
    base::Value::Dict info;
    info.Set("kind", "networkRequest");
    info.Set("requestId", requestId);
    std::string json;
    base::JSONWriter::Write(info, &json);
    recordreplay::NewDependencyGraphNode(json.c_str());
  }

  base::DictionaryValue dict;

  dict.SetDoubleKey("bookmark", (double) bookmark);
  dict.SetString("requestId", requestId);
  dict.SetString("requestUrl", url_string);
  dict.SetString("requestMethod", request.HttpMethod().Utf8());
  const char* requestCause = GetRequestCauseString(request);
  if (requestCause) {
    dict.SetString("requestCause", requestCause);
  }

  base::ListValue headers;
  for (auto header : request.HttpHeaderFields()) {
    base::DictionaryValue header_obj;
    header_obj.SetString("name", header.key.Utf8());
    header_obj.SetString("value", header.value.Utf8());
    headers.Append(std::move(header_obj));
  }
  dict.SetKey("requestHeaders", std::move(headers));

  if (resource) {
    const blink::FetchInitiatorInfo& initiator_info = resource->Options().initiator_info;
    absl::optional<base::DictionaryValue> initiator_obj = BuildInitiatorObject(document, initiator_info);
    if (initiator_obj) {
      dict.SetKey("initiator", *std::move(initiator_obj));
    }
  }

  BrowserEvent("Network.PrepareRequest", dict);

  // Check the request body for request data or stream.
  const scoped_refptr<blink::EncodedFormData>& form_body =
    request.Body().FormBody();
  if (form_body) {
    WTF::String data = form_body->FlattenToString();
    base::DictionaryValue requestDataDict;
    requestDataDict.SetString("requestId", requestId);
    std::string dataStr = data.Utf8();
    requestDataDict.SetString("data", dataStr);
    requestDataDict.SetInteger("dataLength", (int)dataStr.size());
    BrowserEvent("Network.RequestData.Form", requestDataDict);
  }
}

void OnNetworkResourceRedirect(uint64_t inspector_id, const blink::KURL& new_url,
                               blink::ResourceRequest* new_request) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  base::DictionaryValue dict;
  dict.SetString("requestId", RecordReplayNetworkRequestId(inspector_id));
  dict.SetString("requestUrl", new_url.GetString().Utf8());

  base::ListValue headers;
  if (new_request) {
    for (auto header : new_request->HttpHeaderFields()) {
      base::DictionaryValue header_obj;
      header_obj.SetString("name", header.key.Utf8());
      header_obj.SetString("value", header.value.Utf8());
      headers.Append(std::move(header_obj));
    }
  }
  dict.SetKey("requestHeaders", std::move(headers));

  BrowserEvent("Network.ResourceRedirect", dict);
}

void OnNetworkReceiveResponse(uint64_t inspector_id,
                              const blink::ResourceResponse& response) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  base::DictionaryValue dict;
  dict.SetString("requestId", RecordReplayNetworkRequestId(inspector_id));
  const char* http_version = HttpVersionToString(response.HttpVersion());
  base::ListValue headers;
  for (auto header : response.HttpHeaderFields()) {
    base::DictionaryValue header_obj;
    header_obj.SetString("name", header.key.Utf8());
    header_obj.SetString("value", header.value.Utf8());
    headers.Append(std::move(header_obj));
  }
  dict.SetKey("responseHeaders", std::move(headers));
  dict.SetString("responseProtocolVersion", http_version);
  dict.SetDoubleKey("responseStatus", response.HttpStatusCode());
  dict.SetString("responseStatusText", response.HttpStatusText().Utf8());
  dict.SetBoolean("responseFromCache", response.WasCached());
  BrowserEvent("Network.DidReceiveResponse", dict);
}

void OnNetworkReceiveData(uint64_t inspector_id, const char* data, int length) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  std::string requestId = RecordReplayNetworkRequestId(inspector_id);

  if (recordreplay::DependencyGraphEnabled()) {
    base::Value::Dict info;
    info.Set("kind", "networkReceiveData");
    info.Set("requestId", requestId);
    info.Set("length", length);
    std::string json;
    base::JSONWriter::Write(info, &json);
    recordreplay::NewDependencyGraphNode(json.c_str());
  }

  base::DictionaryValue dict;
  dict.SetString("requestId", requestId);
  dict.SetDoubleKey("dataLength", (double) length);
  if (data) {
    std::string data_base64 = base::Base64Encode(
      base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data),
        length
      )
    );
    dict.SetString("data", data_base64);
  }
  BrowserEvent("Network.DidReceiveData", dict);
}

void OnNetworkFinishLoading(uint64_t inspector_id,
                            int64_t encoded_body_length,
                            int64_t decoded_body_length) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  std::string requestId = RecordReplayNetworkRequestId(inspector_id);

  if (recordreplay::DependencyGraphEnabled()) {
    base::Value::Dict info;
    info.Set("kind", "networkFinishLoading");
    info.Set("requestId", requestId);
    std::string json;
    base::JSONWriter::Write(info, &json);
    recordreplay::NewDependencyGraphNode(json.c_str());
  }

  base::DictionaryValue dict;
  dict.SetString("requestId", requestId);
  dict.SetDoubleKey("encodedBodySize", (double) encoded_body_length);
  dict.SetDoubleKey("decodedBodySize", (double) decoded_body_length);
  BrowserEvent("Network.DidFinishLoading", dict);
}

void OnNetworkFail(uint64_t inspector_id, const blink::WebURLError& error) {
  if (!PermitRecordReplayBrowserEvents()) {
    return;
  }

  std::string requestId = RecordReplayNetworkRequestId(inspector_id);

  if (recordreplay::DependencyGraphEnabled()) {
    base::Value::Dict info;
    info.Set("kind", "networkFail");
    info.Set("requestId", requestId);
    std::string json;
    base::JSONWriter::Write(info, &json);
    recordreplay::NewDependencyGraphNode(json.c_str());
  }

  std::string reason = net::ErrorToShortString(error.reason());
  base::DictionaryValue dict;
  dict.SetString("requestId", requestId);
  dict.SetString("requestFailedReason", std::move(reason));
  BrowserEvent("Network.DidFailLoading", dict);
}

}  // namespace recordreplay
