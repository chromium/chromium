// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::protocol::Array;
// Renaming Cache since there is another blink::Cache.
using ProtocolCache = blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::CachedResponse;
using blink::protocol::CacheStorage::CachedResponseType;
using blink::protocol::CacheStorage::DataEntry;
using blink::protocol::CacheStorage::Header;
// Renaming Response since there is another blink::Response.
using ProtocolResponse = blink::protocol::Response;

using DeleteCacheCallback =
    blink::protocol::CacheStorage::Backend::DeleteCacheCallback;
using DeleteEntryCallback =
    blink::protocol::CacheStorage::Backend::DeleteEntryCallback;
using RequestCacheNamesCallback =
    blink::protocol::CacheStorage::Backend::RequestCacheNamesCallback;
using RequestEntriesCallback =
    blink::protocol::CacheStorage::Backend::RequestEntriesCallback;
using RequestCachedResponseCallback =
    blink::protocol::CacheStorage::Backend::RequestCachedResponseCallback;

namespace blink {

namespace {

String BuildCacheId(const String& storage_key, const String& cache_name) {
  DCHECK(storage_key.find('|') == WTF::kNotFound);
  StringBuilder id;
  id.Append(storage_key);
  id.Append('|');
  id.Append(cache_name);
  return id.ToString();
}

ProtocolResponse ParseCacheId(const String& id,
                              String* storage_key,
                              String* cache_name) {
  wtf_size_t pipe = id.find('|');
  if (pipe == WTF::kNotFound)
    return ProtocolResponse::ServerError("Invalid cache id");
  *storage_key = id.Substring(0, pipe);
  *cache_name = id.Substring(pipe + 1);

  absl::optional<StorageKey> key =
      StorageKey::Deserialize(StringUTF8Adaptor(*storage_key).AsStringPiece());
  if (!key.has_value()) {
    return ProtocolResponse::ServerError("Not able to deserialize storage key");
  }
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromUrlOrigin(key->origin());

  if (!origin->IsPotentiallyTrustworthy()) {
    return ProtocolResponse::ServerError(
        origin->IsPotentiallyTrustworthyErrorMessage().Utf8());
  }
  return ProtocolResponse::Success();
}

ProtocolResponse GetExecutionContext(InspectedFrames* frames,
                                     const String& storage_key,
                                     ExecutionContext** context) {
  LocalFrame* frame = frames->FrameWithStorageKey(storage_key);
  if (!frame) {
    return ProtocolResponse::InvalidParams(
        "No frame found for given storage key");
  }

  *context = frame->DomWindow();

  return ProtocolResponse::Success();
}

ProtocolResponse AssertCacheStorage(
    const String& storage_key,
    InspectedFrames* frames,
    InspectorCacheStorageAgent::CachesMap* caches,
    mojom::blink::CacheStorage** result) {
  ExecutionContext* context = nullptr;
  ProtocolResponse response =
      GetExecutionContext(frames, storage_key, &context);
  if (!response.IsSuccess())
    return response;

  auto it = caches->find(storage_key);

  if (it == caches->end()) {
    mojo::Remote<mojom::blink::CacheStorage> cache_storage_remote;
    context->GetBrowserInterfaceBroker().GetInterface(
        cache_storage_remote.BindNewPipeAndPassReceiver(
            frames->Root()->GetTaskRunner(TaskType::kFileReading)));
    *result = cache_storage_remote.get();
    caches->Set(storage_key, std::move(cache_storage_remote));
  } else {
    *result = it->value.get();
  }

  return ProtocolResponse::Success();
}

ProtocolResponse AssertCacheStorageAndNameForId(
    const String& cache_id,
    InspectedFrames* frames,
    String* cache_name,
    InspectorCacheStorageAgent::CachesMap* caches,
    mojom::blink::CacheStorage** result) {
  String storage_key;
  ProtocolResponse response = ParseCacheId(cache_id, &storage_key, cache_name);

  if (!response.IsSuccess())
    return response;
  return AssertCacheStorage(storage_key, frames, caches, result);
}

const char* CacheStorageErrorString(mojom::blink::CacheStorageError error) {
  switch (error) {
    case mojom::blink::CacheStorageError::kErrorNotImplemented:
      return "not implemented.";
    case mojom::blink::CacheStorageError::kErrorNotFound:
      return "not found.";
    case mojom::blink::CacheStorageError::kErrorExists:
      return "cache already exists.";
    case mojom::blink::CacheStorageError::kErrorQuotaExceeded:
      return "quota exceeded.";
    case mojom::blink::CacheStorageError::kErrorCacheNameNotFound:
      return "cache not found.";
    case mojom::blink::CacheStorageError::kErrorQueryTooLarge:
      return "operation too large.";
    case mojom::blink::CacheStorageError::kErrorStorage:
      return "storage failure.";
    case mojom::blink::CacheStorageError::kErrorDuplicateOperation:
      return "duplicate operation.";
    case mojom::blink::CacheStorageError::kErrorCrossOriginResourcePolicy:
      return "failed Cross-Origin-Resource-Policy check.";
    case mojom::blink::CacheStorageError::kSuccess:
      // This function should only be called upon error.
      break;
  }
  NOTREACHED();
  return "";
}

CachedResponseType ResponseTypeToString(
    network::mojom::FetchResponseType type) {
  switch (type) {
    case network::mojom::FetchResponseType::kBasic:
      return protocol::CacheStorage::CachedResponseTypeEnum::Basic;
    case network::mojom::FetchResponseType::kCors:
      return protocol::CacheStorage::CachedResponseTypeEnum::Cors;
    case network::mojom::FetchResponseType::kDefault:
      return protocol::CacheStorage::CachedResponseTypeEnum::Default;
    case network::mojom::FetchResponseType::kError:
      return protocol::CacheStorage::CachedResponseTypeEnum::Error;
    case network::mojom::FetchResponseType::kOpaque:
      return protocol::CacheStorage::CachedResponseTypeEnum::OpaqueResponse;
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return protocol::CacheStorage::CachedResponseTypeEnum::OpaqueRedirect;
  }
  NOTREACHED();
  return "";
}

struct DataRequestParams {
  String cache_name;
  int skip_count;
  // If set to -1, pagination is disabled and all available requests are
  // returned.
  int page_size;
  String path_filter;
};

struct RequestResponse {
  String request_url;
  String request_method;
  HTTPHeaderMap request_headers;
  int response_status;
  String response_status_text;
  double response_time;
  network::mojom::FetchResponseType response_type;
  HTTPHeaderMap response_headers;
};

class ResponsesAccumulator : public RefCounted<ResponsesAccumulator> {
 public:
  ResponsesAccumulator(
      wtf_size_t num_responses,
      const DataRequestParams& params,
      mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
          cache_pending_remote,
      std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params),
        num_responses_left_(num_responses),
        cache_remote_(std::move(cache_pending_remote)),
        callback_(std::move(callback)) {}

  ResponsesAccumulator(const ResponsesAccumulator&) = delete;
  ResponsesAccumulator& operator=(const ResponsesAccumulator&) = delete;

  void Dispatch(Vector<mojom::blink::FetchAPIRequestPtr> old_requests) {
    int64_t trace_id = blink::cache_storage::CreateTraceId();
    TRACE_EVENT_WITH_FLOW0("CacheStorage", "ResponsesAccumulator::Dispatch",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_OUT);

    Vector<mojom::blink::FetchAPIRequestPtr> requests;
    if (params_.path_filter.empty()) {
      requests = std::move(old_requests);
    } else {
      for (auto& request : old_requests) {
        String urlPath(request->url.GetPath());
        if (!urlPath.Contains(params_.path_filter,
                              WTF::kTextCaseUnicodeInsensitive))
          continue;
        requests.push_back(std::move(request));
      }
    }
    wtf_size_t requestSize = requests.size();
    if (!requestSize) {
      callback_->sendSuccess(std::make_unique<Array<DataEntry>>(), 0);
      return;
    }

    responses_ = Vector<RequestResponse>(requestSize);
    num_responses_left_ = requestSize;
    for (auto& request : requests) {
      // All FetchAPIRequests in cache_storage code are supposed to not contain
      // a body.
      DCHECK(!request->blob && request->body.IsEmpty());
      auto request_clone_without_body = mojom::blink::FetchAPIRequest::New(
          request->mode, request->is_main_resource_load, request->destination,
          request->frame_type, request->url, request->method, request->headers,
          nullptr /* blob */, ResourceRequestBody(), request->request_initiator,
          request->navigation_redirect_chain, request->referrer.Clone(),
          request->credentials_mode, request->cache_mode,
          request->redirect_mode, request->integrity, request->priority,
          request->fetch_window_id, request->keepalive, request->is_reload,
          request->is_history_navigation, request->devtools_stack_id,
          request->trust_token_params.Clone(), request->target_address_space);
      cache_remote_->Match(
          std::move(request), mojom::blink::CacheQueryOptions::New(),
          /*in_related_fetch_event=*/false, /*in_range_fetch_event=*/false,
          trace_id,
          WTF::BindOnce(
              [](scoped_refptr<ResponsesAccumulator> accumulator,
                 mojom::blink::FetchAPIRequestPtr request,
                 mojom::blink::MatchResultPtr result) {
                if (result->is_status()) {
                  accumulator->SendFailure(result->get_status());
                } else {
                  accumulator->AddRequestResponsePair(request,
                                                      result->get_response());
                }
              },
              scoped_refptr<ResponsesAccumulator>(this),
              std::move(request_clone_without_body)));
    }
  }

  void AddRequestResponsePair(
      const mojom::blink::FetchAPIRequestPtr& request,
      const mojom::blink::FetchAPIResponsePtr& response) {
    DCHECK_GT(num_responses_left_, 0);
    RequestResponse& next_request_response =
        responses_.at(responses_.size() - num_responses_left_);

    next_request_response.request_url = request->url.GetString();
    next_request_response.request_method = request->method;
    for (const auto& header : request->headers) {
      next_request_response.request_headers.Set(AtomicString(header.key),
                                                AtomicString(header.value));
    }

    next_request_response.response_status = response->status_code;
    next_request_response.response_status_text = response->status_text;
    next_request_response.response_time = response->response_time.ToDoubleT();
    next_request_response.response_type = response->response_type;
    for (const auto& header : response->headers) {
      next_request_response.response_headers.Set(AtomicString(header.key),
                                                 AtomicString(header.value));
    }

    if (--num_responses_left_ != 0)
      return;

    std::sort(responses_.begin(), responses_.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::CodeUnitCompareLessThan(a.request_url,
                                                    b.request_url);
              });
    size_t returned_entries_count = responses_.size();
    if (params_.skip_count > 0)
      responses_.EraseAt(0, params_.skip_count);
    if (params_.page_size != -1 &&
        static_cast<size_t>(params_.page_size) < responses_.size()) {
      responses_.EraseAt(params_.page_size,
                         responses_.size() - params_.page_size);
    }
    auto array = std::make_unique<protocol::Array<DataEntry>>();
    for (const auto& request_response : responses_) {
      std::unique_ptr<DataEntry> entry =
          DataEntry::create()
              .setRequestURL(request_response.request_url)
              .setRequestMethod(request_response.request_method)
              .setRequestHeaders(
                  SerializeHeaders(request_response.request_headers))
              .setResponseStatus(request_response.response_status)
              .setResponseStatusText(request_response.response_status_text)
              .setResponseTime(request_response.response_time)
              .setResponseType(
                  ResponseTypeToString(request_response.response_type))
              .setResponseHeaders(
                  SerializeHeaders(request_response.response_headers))
              .build();
      array->emplace_back(std::move(entry));
    }

    callback_->sendSuccess(std::move(array), returned_entries_count);
  }

  void SendFailure(const mojom::blink::CacheStorageError& error) {
    callback_->sendFailure(ProtocolResponse::ServerError(
        String::Format("Error requesting responses for cache %s : %s",
                       params_.cache_name.Latin1().c_str(),
                       CacheStorageErrorString(error))
            .Utf8()));
  }

  std::unique_ptr<Array<Header>> SerializeHeaders(
      const HTTPHeaderMap& headers) {
    auto result = std::make_unique<protocol::Array<Header>>();
    for (HTTPHeaderMap::const_iterator it = headers.begin(),
                                       end = headers.end();
         it != end; ++it) {
      result->emplace_back(
          Header::create().setName(it->key).setValue(it->value).build());
    }
    return result;
  }

 private:
  DataRequestParams params_;
  int num_responses_left_;
  Vector<RequestResponse> responses_;
  mojo::AssociatedRemote<mojom::blink::CacheStorageCache> cache_remote_;
  std::unique_ptr<RequestEntriesCallback> callback_;
};

class GetCacheKeysForRequestData {
  USING_FAST_MALLOC(GetCacheKeysForRequestData);

 public:
  GetCacheKeysForRequestData(
      const DataRequestParams& params,
      mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
          cache_pending_remote,
      std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params), callback_(std::move(callback)) {
    cache_remote_.Bind(std::move(cache_pending_remote));
  }

  GetCacheKeysForRequestData(const GetCacheKeysForRequestData&) = delete;
  GetCacheKeysForRequestData& operator=(const GetCacheKeysForRequestData&) =
      delete;

  void Dispatch(std::unique_ptr<GetCacheKeysForRequestData> self) {
    int64_t trace_id = blink::cache_storage::CreateTraceId();
    TRACE_EVENT_WITH_FLOW0(
        "CacheStorage", "GetCacheKeysForRequestData::Dispatch",
        TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
    cache_remote_->Keys(
        nullptr /* request */, mojom::blink::CacheQueryOptions::New(), trace_id,
        WTF::BindOnce(
            [](DataRequestParams params,
               std::unique_ptr<GetCacheKeysForRequestData> self,
               mojom::blink::CacheKeysResultPtr result) {
              if (result->is_status()) {
                self->callback_->sendFailure(ProtocolResponse::ServerError(
                    String::Format(
                        "Error requesting requests for cache %s: %s",
                        params.cache_name.Latin1().c_str(),
                        CacheStorageErrorString(result->get_status()))
                        .Utf8()));
              } else {
                if (result->get_keys().empty()) {
                  auto array = std::make_unique<protocol::Array<DataEntry>>();
                  self->callback_->sendSuccess(std::move(array), 0);
                  return;
                }
                scoped_refptr<ResponsesAccumulator> accumulator =
                    base::AdoptRef(new ResponsesAccumulator(
                        result->get_keys().size(), params,
                        self->cache_remote_.Unbind(),
                        std::move(self->callback_)));
                accumulator->Dispatch(std::move(result->get_keys()));
              }
            },
            params_, std::move(self)));
  }

 private:
  DataRequestParams params_;
  mojo::AssociatedRemote<mojom::blink::CacheStorageCache> cache_remote_;
  std::unique_ptr<RequestEntriesCallback> callback_;
};

class CachedResponseFileReaderLoaderClient final
    : private FileReaderLoaderClient {
 public:
  static void Load(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   scoped_refptr<BlobDataHandle> blob,
                   std::unique_ptr<RequestCachedResponseCallback> callback) {
    new CachedResponseFileReaderLoaderClient(
        std::move(task_runner), std::move(blob), std::move(callback));
  }

  CachedResponseFileReaderLoaderClient(
      const CachedResponseFileReaderLoaderClient&) = delete;
  CachedResponseFileReaderLoaderClient& operator=(
      const CachedResponseFileReaderLoaderClient&) = delete;

  void DidStartLoading() override {}

  void DidFinishLoading() override {
    std::unique_ptr<CachedResponse> response =
        CachedResponse::create()
            .setBody(protocol::Binary::fromSharedBuffer(data_))
            .build();
    callback_->sendSuccess(std::move(response));
    dispose();
  }

  void DidFail(FileErrorCode error) override {
    callback_->sendFailure(ProtocolResponse::ServerError(
        String::Format("Unable to read the cached response, error code: %d",
                       static_cast<int>(error))
            .Utf8()));
    dispose();
  }

  void DidReceiveDataForClient(const char* data,
                               unsigned data_length) override {
    data_->Append(data, data_length);
  }

 private:
  CachedResponseFileReaderLoaderClient(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<BlobDataHandle>&& blob,
      std::unique_ptr<RequestCachedResponseCallback>&& callback)
      : loader_(std::make_unique<FileReaderLoader>(
            FileReaderLoader::kReadByClient,
            static_cast<FileReaderLoaderClient*>(this),
            std::move(task_runner))),
        callback_(std::move(callback)),
        data_(SharedBuffer::Create()) {
    loader_->Start(std::move(blob));
  }

  ~CachedResponseFileReaderLoaderClient() override = default;

  void dispose() { delete this; }

  std::unique_ptr<FileReaderLoader> loader_;
  std::unique_ptr<RequestCachedResponseCallback> callback_;
  scoped_refptr<SharedBuffer> data_;
};

}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent(InspectedFrames* frames)
    : frames_(frames) {}

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

void InspectorCacheStorageAgent::Trace(Visitor* visitor) const {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorCacheStorageAgent::requestCacheNames(
    protocol::Maybe<String> maybe_security_origin,
    protocol::Maybe<String> maybe_storage_key,
    std::unique_ptr<RequestCacheNamesCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::requestCacheNames",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  if (maybe_security_origin.isJust() == maybe_storage_key.isJust()) {
    callback->sendFailure(ProtocolResponse::InvalidParams(
        "At least and at most one of security_origin, "
        "storage_key must be specified"));
    return;
  }
  String storage_key, security_origin;
  if (maybe_storage_key.isJust()) {
    storage_key = maybe_storage_key.fromJust();
    absl::optional<StorageKey> key =
        StorageKey::Deserialize(StringUTF8Adaptor(storage_key).AsStringPiece());
    if (!key.has_value()) {
      callback->sendFailure(ProtocolResponse::InvalidParams(
          "Not able to deserialize storage key"));
      return;
    }
    security_origin =
        SecurityOrigin::CreateFromUrlOrigin(key->origin())->ToString();

    if (!security_origin.StartsWith("http")) {
      callback->sendFailure(ProtocolResponse::InvalidParams(
          "Storage key corresponds to invalid origin"));
      return;
    }
  } else {
    security_origin = maybe_security_origin.fromJust();
    if (!security_origin.StartsWith("http")) {
      callback->sendFailure(
          ProtocolResponse::InvalidParams("Invalid security origin"));
      return;
    }
    scoped_refptr<SecurityOrigin> sec_origin =
        SecurityOrigin::CreateFromString(security_origin);
    // Cache Storage API is restricted to trustworthy origins.
    if (!sec_origin->IsPotentiallyTrustworthy()) {
      // Don't treat this as an error, just don't attempt to open and enumerate
      // the caches.
      callback->sendSuccess(std::make_unique<protocol::Array<ProtocolCache>>());
      return;
    }
    storage_key =
        WTF::String(StorageKey(sec_origin->ToUrlOrigin()).Serialize());
  }

  mojom::blink::CacheStorage* cache_storage = nullptr;

  ProtocolResponse response =
      AssertCacheStorage(storage_key, frames_, &caches_, &cache_storage);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  cache_storage->Keys(
      trace_id, WTF::BindOnce(
                    [](String security_origin, String storage_key,
                       std::unique_ptr<RequestCacheNamesCallback> callback,
                       const Vector<String>& caches) {
                      auto array =
                          std::make_unique<protocol::Array<ProtocolCache>>();
                      for (auto& cache : caches) {
                        array->emplace_back(
                            ProtocolCache::create()
                                .setSecurityOrigin(security_origin)
                                .setStorageKey(storage_key)
                                .setCacheName(cache)
                                .setCacheId(BuildCacheId(storage_key, cache))
                                .build());
                      }
                      callback->sendSuccess(std::move(array));
                    },
                    security_origin, storage_key, std::move(callback)));
}

void InspectorCacheStorageAgent::requestEntries(
    const String& cache_id,
    protocol::Maybe<int> skip_count,
    protocol::Maybe<int> page_size,
    protocol::Maybe<String> path_filter,
    std::unique_ptr<RequestEntriesCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::requestEntries",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  DataRequestParams params;
  params.cache_name = cache_name;
  params.page_size = page_size.fromMaybe(-1);
  params.skip_count = skip_count.fromMaybe(0);
  params.path_filter = path_filter.fromMaybe("");

  cache_storage->Open(
      cache_name, trace_id,
      WTF::BindOnce(
          [](DataRequestParams params,
             std::unique_ptr<RequestEntriesCallback> callback,
             mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::ServerError(
                  String::Format("Error requesting cache %s: %s",
                                 params.cache_name.Latin1().c_str(),
                                 CacheStorageErrorString(result->get_status()))
                      .Utf8()));
            } else {
              auto request = std::make_unique<GetCacheKeysForRequestData>(
                  params, std::move(result->get_cache()), std::move(callback));
              auto* request_ptr = request.get();
              request_ptr->Dispatch(std::move(request));
            }
          },
          params, std::move(callback)));
}

void InspectorCacheStorageAgent::deleteCache(
    const String& cache_id,
    std::unique_ptr<DeleteCacheCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::deleteCache",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache_storage->Delete(
      cache_name, trace_id,
      WTF::BindOnce(
          [](std::unique_ptr<DeleteCacheCallback> callback,
             mojom::blink::CacheStorageError error) {
            if (error == mojom::blink::CacheStorageError::kSuccess) {
              callback->sendSuccess();
            } else {
              callback->sendFailure(ProtocolResponse::ServerError(
                  String::Format("Error requesting cache names: %s",
                                 CacheStorageErrorString(error))
                      .Utf8()));
            }
          },
          std::move(callback)));
}

void InspectorCacheStorageAgent::deleteEntry(
    const String& cache_id,
    const String& request,
    std::unique_ptr<DeleteEntryCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::deleteEntry",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache_storage->Open(
      cache_name, trace_id,
      WTF::BindOnce(
          [](String cache_name, String request, int64_t trace_id,
             std::unique_ptr<DeleteEntryCallback> callback,
             mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::ServerError(
                  String::Format("Error requesting cache %s: %s",
                                 cache_name.Latin1().c_str(),
                                 CacheStorageErrorString(result->get_status()))
                      .Utf8()));
            } else {
              Vector<mojom::blink::BatchOperationPtr> batch_operations;
              batch_operations.push_back(mojom::blink::BatchOperation::New());
              auto& operation = batch_operations.back();
              operation->operation_type = mojom::blink::OperationType::kDelete;
              operation->request = mojom::blink::FetchAPIRequest::New();
              operation->request->url = KURL(request);
              operation->request->method = String("GET");

              mojo::AssociatedRemote<mojom::blink::CacheStorageCache>
                  cache_remote;
              cache_remote.Bind(std::move(result->get_cache()));
              auto* cache = cache_remote.get();
              cache->Batch(
                  std::move(batch_operations), trace_id,
                  WTF::BindOnce(
                      [](mojo::AssociatedRemote<mojom::blink::CacheStorageCache>
                             cache_remote,
                         std::unique_ptr<DeleteEntryCallback> callback,
                         mojom::blink::CacheStorageVerboseErrorPtr error) {
                        if (error->value !=
                            mojom::blink::CacheStorageError::kSuccess) {
                          callback->sendFailure(ProtocolResponse::ServerError(
                              String::Format(
                                  "Error deleting cache entry: %s",
                                  CacheStorageErrorString(error->value))
                                  .Utf8()));
                        } else {
                          callback->sendSuccess();
                        }
                      },
                      std::move(cache_remote), std::move(callback)));
            }
          },
          cache_name, request, trace_id, std::move(callback)));
}

void InspectorCacheStorageAgent::requestCachedResponse(
    const String& cache_id,
    const String& request_url,
    const std::unique_ptr<protocol::Array<protocol::CacheStorage::Header>>
        request_headers,
    std::unique_ptr<RequestCachedResponseCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::requestCachedResponse",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  auto request = mojom::blink::FetchAPIRequest::New();
  request->url = KURL(request_url);
  request->method = String("GET");
  for (const std::unique_ptr<protocol::CacheStorage::Header>& header :
       *request_headers) {
    request->headers.insert(header->getName(), header->getValue());
  }

  auto task_runner = frames_->Root()->GetTaskRunner(TaskType::kFileReading);
  auto multi_query_options = mojom::blink::MultiCacheQueryOptions::New();
  multi_query_options->query_options = mojom::blink::CacheQueryOptions::New();
  multi_query_options->cache_name = cache_name;

  cache_storage->Match(
      std::move(request), std::move(multi_query_options),
      /*in_related_fetch_event=*/false, /*in_range_fetch_event=*/false,
      trace_id,
      WTF::BindOnce(
          [](std::unique_ptr<RequestCachedResponseCallback> callback,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             mojom::blink::MatchResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::ServerError(
                  String::Format("Unable to read cached response: %s",
                                 CacheStorageErrorString(result->get_status()))
                      .Utf8()));
            } else {
              std::unique_ptr<protocol::DictionaryValue> headers =
                  protocol::DictionaryValue::create();
              if (!result->get_response()->blob) {
                callback->sendSuccess(CachedResponse::create()
                                          .setBody(protocol::Binary())
                                          .build());
                return;
              }
              CachedResponseFileReaderLoaderClient::Load(
                  std::move(task_runner),
                  std::move(result->get_response()->blob), std::move(callback));
            }
          },
          std::move(callback), std::move(task_runner)));
}
}  // namespace blink
