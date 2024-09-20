// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"

#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket_manager.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

String BuildCacheId(const String& storage_key,
                    const std::optional<String>& storage_bucket_name,
                    const String& cache_name) {
  DCHECK(storage_key.find('|') == WTF::kNotFound);
  StringBuilder id;
  id.Append(storage_key);
  if (storage_bucket_name.has_value()) {
    id.Append('|');
    id.Append(storage_bucket_name.value());
  }
  id.Append('|');
  id.Append(cache_name);
  return id.ToString();
}

ProtocolResponse ParseCacheId(const String& id,
                              String* storage_key,
                              std::optional<String>* storage_bucket_name,
                              String* cache_name) {
  Vector<String> id_parts;
  id.Split('|', true, id_parts);
  if (id_parts.size() == 2) {
    *storage_key = id_parts[0];
    *storage_bucket_name = std::nullopt;
    *cache_name = id_parts[1];
  } else if (id_parts.size() == 3) {
    *storage_key = id_parts[0];
    *storage_bucket_name = id_parts[1];
    *cache_name = id_parts[2];
  } else {
    return ProtocolResponse::ServerError("Invalid cache id");
  }

  std::optional<StorageKey> key =
      StorageKey::Deserialize(StringUTF8Adaptor(*storage_key).AsStringView());
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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

// A RefCounted wrapper for a Devtools callback so that it can be referenced in
// multiple locations.
template <typename RequestCallback>
class RequestCallbackWrapper
    : public RefCounted<RequestCallbackWrapper<RequestCallback>> {
 public:
  static scoped_refptr<RequestCallbackWrapper<RequestCallback>> Wrap(
      std::unique_ptr<RequestCallback> request_callback) {
    return AdoptRef(new RequestCallbackWrapper<RequestCallback>(
        std::move(request_callback)));
  }

  template <typename... Args>
  void SendSuccess(Args... args) {
    if (request_callback_) {
      request_callback_->sendSuccess(std::forward<Args>(args)...);
      request_callback_.reset();
    }
  }

  void SendFailure(ProtocolResponse response) {
    if (request_callback_) {
      request_callback_->sendFailure(response);
      request_callback_.reset();
    }
  }

  base::OnceCallback<void(ProtocolResponse)> GetFailureCallback() {
    return WTF::BindOnce(
        [](scoped_refptr<RequestCallbackWrapper<RequestCallback>>
               callback_wrapper,
           ProtocolResponse response) {
          callback_wrapper->SendFailure(response);
        },
        WrapRefCounted(this));
  }

 private:
  explicit RequestCallbackWrapper(
      std::unique_ptr<RequestCallback> request_callback)
      : request_callback_(std::move(request_callback)) {}

  std::unique_ptr<RequestCallback> request_callback_;
};

class ResponsesAccumulator : public RefCounted<ResponsesAccumulator> {
 public:
  ResponsesAccumulator(
      wtf_size_t num_responses,
      const DataRequestParams& params,
      mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
          cache_pending_remote,
      scoped_refptr<RequestCallbackWrapper<RequestEntriesCallback>>
          callback_wrapper)
      : params_(params),
        num_responses_left_(num_responses),
        cache_remote_(std::move(cache_pending_remote)),
        callback_wrapper_(std::move(callback_wrapper)) {}

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
        String urlPath(request->url.GetPath().ToString());
        if (!urlPath.Contains(params_.path_filter,
                              WTF::kTextCaseUnicodeInsensitive)) {
          continue;
        }
        requests.push_back(std::move(request));
      }
    }
    wtf_size_t requestSize = requests.size();
    if (!requestSize) {
      callback_wrapper_->SendSuccess(std::make_unique<Array<DataEntry>>(), 0);
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
          request->trust_token_params.Clone(), request->target_address_space,
          request->attribution_reporting_eligibility,
          request->attribution_reporting_support,
          /*service_worker_race_network_request_token=*/std::nullopt);
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
    next_request_response.response_time =
        response->response_time.InSecondsFSinceUnixEpoch();
    next_request_response.response_type = response->response_type;
    for (const auto& header : response->headers) {
      next_request_response.response_headers.Set(AtomicString(header.key),
                                                 AtomicString(header.value));
    }

    if (--num_responses_left_ != 0) {
      return;
    }

    std::sort(responses_.begin(), responses_.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::CodeUnitCompareLessThan(a.request_url,
                                                    b.request_url);
              });
    size_t returned_entries_count = responses_.size();
    if (params_.skip_count > 0) {
      responses_.EraseAt(0, params_.skip_count);
    }
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

    callback_wrapper_->SendSuccess(std::move(array), returned_entries_count);
  }

  void SendFailure(const mojom::blink::CacheStorageError& error) {
    callback_wrapper_->SendFailure(ProtocolResponse::ServerError(
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
  scoped_refptr<RequestCallbackWrapper<RequestEntriesCallback>>
      callback_wrapper_;
};

class GetCacheKeysForRequestData {
  USING_FAST_MALLOC(GetCacheKeysForRequestData);

 public:
  GetCacheKeysForRequestData(
      const DataRequestParams& params,
      mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
          cache_pending_remote,
      scoped_refptr<RequestCallbackWrapper<RequestEntriesCallback>>
          callback_wrapper)
      : params_(params), callback_wrapper_(std::move(callback_wrapper)) {
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
                self->callback_wrapper_->SendFailure(
                    ProtocolResponse::ServerError(
                        String::Format(
                            "Error requesting requests for cache %s: %s",
                            params.cache_name.Latin1().c_str(),
                            CacheStorageErrorString(result->get_status()))
                            .Utf8()));
              } else {
                if (result->get_keys().empty()) {
                  auto array = std::make_unique<protocol::Array<DataEntry>>();
                  self->callback_wrapper_->SendSuccess(std::move(array), 0);
                  return;
                }
                scoped_refptr<ResponsesAccumulator> accumulator =
                    base::AdoptRef(new ResponsesAccumulator(
                        result->get_keys().size(), params,
                        self->cache_remote_.Unbind(), self->callback_wrapper_));
                accumulator->Dispatch(std::move(result->get_keys()));
              }
            },
            params_, std::move(self)));
  }

 private:
  DataRequestParams params_;
  mojo::AssociatedRemote<mojom::blink::CacheStorageCache> cache_remote_;
  scoped_refptr<RequestCallbackWrapper<RequestEntriesCallback>>
      callback_wrapper_;
};

class CachedResponseFileReaderLoaderClient final
    : public GarbageCollected<CachedResponseFileReaderLoaderClient>,
      public FileReaderClient {
 public:
  static void Load(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<BlobDataHandle> blob,
      scoped_refptr<RequestCallbackWrapper<RequestCachedResponseCallback>>
          callback_wrapper) {
    MakeGarbageCollected<CachedResponseFileReaderLoaderClient>(
        std::move(task_runner), std::move(blob), callback_wrapper);
  }

  CachedResponseFileReaderLoaderClient(
      const CachedResponseFileReaderLoaderClient&) = delete;
  CachedResponseFileReaderLoaderClient& operator=(
      const CachedResponseFileReaderLoaderClient&) = delete;

  FileErrorCode DidStartLoading(uint64_t) override {
    return FileErrorCode::kOK;
  }

  void DidFinishLoading() override {
    std::unique_ptr<CachedResponse> response =
        CachedResponse::create()
            .setBody(protocol::Binary::fromVector(
                std::move(data_).CopyAs<Vector<uint8_t>>()))
            .build();
    callback_wrapper_->SendSuccess(std::move(response));
    dispose();
  }

  void DidFail(FileErrorCode error) override {
    callback_wrapper_->SendFailure(ProtocolResponse::ServerError(
        String::Format("Unable to read the cached response, error code: %d",
                       static_cast<int>(error))
            .Utf8()));
    dispose();
  }

  FileErrorCode DidReceiveData(base::span<const uint8_t> data) override {
    data_.Append(data);
    return FileErrorCode::kOK;
  }

  void Trace(Visitor* visitor) const override {
    FileReaderClient::Trace(visitor);
    visitor->Trace(loader_);
  }

  CachedResponseFileReaderLoaderClient(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<BlobDataHandle>&& blob,
      scoped_refptr<RequestCallbackWrapper<RequestCachedResponseCallback>>
          callback_wrapper)
      : loader_(MakeGarbageCollected<FileReaderLoader>(this,
                                                       std::move(task_runner))),
        callback_wrapper_(callback_wrapper),
        keep_alive_(this) {
    loader_->Start(std::move(blob));
  }

  ~CachedResponseFileReaderLoaderClient() override = default;

 private:
  void dispose() {
    keep_alive_.Clear();
    loader_ = nullptr;
  }

  Member<FileReaderLoader> loader_;
  scoped_refptr<RequestCallbackWrapper<RequestCachedResponseCallback>>
      callback_wrapper_;
  SegmentedBuffer data_;
  SelfKeepAlive<CachedResponseFileReaderLoaderClient> keep_alive_;
};

}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent(InspectedFrames* frames)
    : frames_(frames) {}

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

void InspectorCacheStorageAgent::Trace(Visitor* visitor) const {
  visitor->Trace(frames_);
  visitor->Trace(caches_);
  InspectorBaseAgent::Trace(visitor);
}

// Gets the cache storage remote associated with the storage key and bucket
// name. If the storage bucket name is not specified, it will get the default
// bucket for the storage key.
base::expected<mojom::blink::CacheStorage*, protocol::Response>
InspectorCacheStorageAgent::GetCacheStorageRemote(
    const String& storage_key,
    const std::optional<String>& storage_bucket_name,
    base::OnceCallback<void(ProtocolResponse)> on_failure_callback) {
  LocalFrame* frame = frames_->FrameWithStorageKey(storage_key);
  if (!frame) {
    return base::unexpected(ProtocolResponse::InvalidParams(
        "No frame found for given storage key"));
  }

  if (storage_bucket_name.has_value()) {
    // Handle a non-default bucket.
    ScriptState* script_state = ToScriptStateForMainWorld(frame);
    if (!script_state) {
      return base::unexpected(ProtocolResponse::InternalError());
    }

    Navigator* navigator = frame->DomWindow()->navigator();
    StorageBucketManager* storage_bucket_manager =
        StorageBucketManager::storageBuckets(*navigator);
    StorageBucket* storage_bucket =
        storage_bucket_manager->GetBucketForDevtools(
            script_state, storage_bucket_name.value());
    if (storage_bucket) {
      DummyExceptionStateForTesting exception_state;
      return storage_bucket->caches(exception_state)
          ->GetRemoteForDevtools(WTF::BindOnce(
              std::move(on_failure_callback),
              ProtocolResponse::ServerError("Couldn't retreive caches")));
    }
    return base::unexpected(
        ProtocolResponse::ServerError("Couldn't retrieve bucket"));
  }

  // Handle the default bucket.
  auto it = caches_.find(storage_key);

  // Cached remotes can become unbound if their associated context is detached.
  // Replace these detached remotes with a new remote when this happens.
  if (it == caches_.end() || !it->value->Value().is_bound()) {
    ExecutionContext* context = frame->DomWindow();
    HeapMojoRemote<mojom::blink::CacheStorage> cache_storage_remote(context);
    context->GetBrowserInterfaceBroker().GetInterface(
        cache_storage_remote.BindNewPipeAndPassReceiver(
            frames_->Root()->GetTaskRunner(TaskType::kFileReading)));
    auto new_iter = caches_.Set(
        storage_key, WrapDisallowNew(std::move(cache_storage_remote)));
    return new_iter.stored_value->value->Value().get();
  }

  return it->value->Value().get();
}

base::expected<mojom::blink::CacheStorage*, protocol::Response>
InspectorCacheStorageAgent::GetCacheStorageRemoteForId(
    const String& cache_id,
    String& cache_name,
    base::OnceCallback<void(ProtocolResponse)> on_failure_callback) {
  String storage_key;
  std::optional<String> storage_bucket_name;
  ProtocolResponse response =
      ParseCacheId(cache_id, &storage_key, &storage_bucket_name, &cache_name);

  if (!response.IsSuccess()) {
    return base::unexpected(response);
  }
  return GetCacheStorageRemote(storage_key, storage_bucket_name,
                               std::move(on_failure_callback));
}

void InspectorCacheStorageAgent::requestCacheNames(
    protocol::Maybe<String> maybe_security_origin,
    protocol::Maybe<String> maybe_storage_key,
    protocol::Maybe<protocol::Storage::StorageBucket> maybe_storage_bucket,
    std::unique_ptr<RequestCacheNamesCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::requestCacheNames",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  if (maybe_security_origin.has_value() + maybe_storage_key.has_value() +
          maybe_storage_bucket.has_value() !=
      1) {
    callback->sendFailure(ProtocolResponse::InvalidParams(
        "At least and at most one of security_origin, "
        "storage_key, storage_bucket must be specified"));
    return;
  }
  String storage_key, security_origin;
  if (maybe_storage_key.has_value() || maybe_storage_bucket.has_value()) {
    storage_key = maybe_storage_key.has_value()
                      ? maybe_storage_key.value()
                      : maybe_storage_bucket.value().getStorageKey();
    std::optional<StorageKey> key =
        StorageKey::Deserialize(StringUTF8Adaptor(storage_key).AsStringView());
    if (!key.has_value()) {
      callback->sendFailure(ProtocolResponse::InvalidParams(
          "Not able to deserialize storage key"));
      return;
    }
    security_origin =
        SecurityOrigin::CreateFromUrlOrigin(key->origin())->ToString();
  } else {
    security_origin = maybe_security_origin.value();
    scoped_refptr<SecurityOrigin> sec_origin =
        SecurityOrigin::CreateFromString(security_origin);
    // Cache Storage API is restricted to trustworthy origins.
    if (!sec_origin->IsPotentiallyTrustworthy()) {
      // Don't treat this as an error, just don't attempt to open and enumerate
      // the caches.
      callback->sendSuccess(std::make_unique<protocol::Array<ProtocolCache>>());
      return;
    }
    storage_key = WTF::String(
        StorageKey::CreateFirstParty(sec_origin->ToUrlOrigin()).Serialize());
  }

  std::optional<WTF::String> bucket_name;
  if (maybe_storage_bucket.has_value() && maybe_storage_bucket->hasName()) {
    bucket_name = maybe_storage_bucket->getName("");
  }

  auto callback_wrapper =
      RequestCallbackWrapper<RequestCacheNamesCallback>::Wrap(
          std::move(callback));

  auto cache_storage = GetCacheStorageRemote(
      storage_key, bucket_name, callback_wrapper->GetFailureCallback());
  if (!cache_storage.has_value()) {
    callback_wrapper->SendFailure(cache_storage.error());
    return;
  }

  cache_storage.value()->Keys(
      trace_id,
      WTF::BindOnce(
          [](String security_origin, String storage_key,
             std::optional<WTF::String> bucket_name,
             protocol::Maybe<protocol::Storage::StorageBucket>
                 maybe_storage_bucket,
             int64_t trace_id,
             scoped_refptr<RequestCallbackWrapper<RequestCacheNamesCallback>>
                 callback,
             const Vector<String>& cache_names) {
            auto array = std::make_unique<protocol::Array<ProtocolCache>>();
            for (auto& cache_name : cache_names) {
              std::unique_ptr<ProtocolCache> protocol_cache =
                  ProtocolCache::create()
                      .setSecurityOrigin(security_origin)
                      .setStorageKey(storage_key)
                      .setCacheName(cache_name)
                      .setCacheId(
                          BuildCacheId(storage_key, bucket_name, cache_name))
                      .build();

              if (maybe_storage_bucket.has_value()) {
                protocol_cache->setStorageBucket(maybe_storage_bucket->Clone());
              }

              array->emplace_back(std::move(protocol_cache));
            }
            callback->SendSuccess(std::move(array));
          },
          security_origin, storage_key, bucket_name,
          std::move(maybe_storage_bucket), trace_id,
          std::move(callback_wrapper)));
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

  auto callback_wrapper =
      RequestCallbackWrapper<RequestEntriesCallback>::Wrap(std::move(callback));

  String cache_name;
  auto cache_storage = GetCacheStorageRemoteForId(
      cache_id, cache_name, callback_wrapper->GetFailureCallback());
  if (!cache_storage.has_value()) {
    callback_wrapper->SendFailure(cache_storage.error());
    return;
  }

  DataRequestParams params;
  params.page_size = page_size.value_or(-1);
  params.skip_count = skip_count.value_or(0);
  params.path_filter = path_filter.value_or("");
  params.cache_name = cache_name;

  cache_storage.value()->Open(
      cache_name, trace_id,
      WTF::BindOnce(
          [](scoped_refptr<RequestCallbackWrapper<RequestEntriesCallback>>
                 callback_wrapper,
             DataRequestParams params, mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback_wrapper->SendFailure(ProtocolResponse::ServerError(
                  String::Format("Error requesting cache %s: %s",
                                 params.cache_name.Latin1().c_str(),
                                 CacheStorageErrorString(result->get_status()))
                      .Utf8()));
            } else {
              auto request = std::make_unique<GetCacheKeysForRequestData>(
                  params, std::move(result->get_cache()),
                  std::move(callback_wrapper));
              auto* request_ptr = request.get();
              request_ptr->Dispatch(std::move(request));
            }
          },
          std::move(callback_wrapper), params));
}

void InspectorCacheStorageAgent::deleteCache(
    const String& cache_id,
    std::unique_ptr<DeleteCacheCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::deleteCache",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  auto callback_wrapper =
      RequestCallbackWrapper<DeleteCacheCallback>::Wrap(std::move(callback));

  String cache_name;
  auto cache_storage = GetCacheStorageRemoteForId(
      cache_id, cache_name, callback_wrapper->GetFailureCallback());
  if (!cache_storage.has_value()) {
    callback_wrapper->SendFailure(cache_storage.error());
    return;
  }
  cache_storage.value()->Delete(
      cache_name, trace_id,
      WTF::BindOnce(
          [](scoped_refptr<RequestCallbackWrapper<DeleteCacheCallback>>
                 callback_wrapper,
             mojom::blink::CacheStorageError error) {
            if (error == mojom::blink::CacheStorageError::kSuccess) {
              callback_wrapper->SendSuccess();
            } else {
              callback_wrapper->SendFailure(ProtocolResponse::ServerError(
                  String::Format("Error requesting cache names: %s",
                                 CacheStorageErrorString(error))
                      .Utf8()));
            }
          },
          std::move(callback_wrapper)));
}

void InspectorCacheStorageAgent::deleteEntry(
    const String& cache_id,
    const String& request,
    std::unique_ptr<DeleteEntryCallback> callback) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "InspectorCacheStorageAgent::deleteEntry",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  auto callback_wrapper =
      RequestCallbackWrapper<DeleteEntryCallback>::Wrap(std::move(callback));

  String cache_name;
  auto cache_storage = GetCacheStorageRemoteForId(
      cache_id, cache_name, callback_wrapper->GetFailureCallback());
  if (!cache_storage.has_value()) {
    callback_wrapper->SendFailure(cache_storage.error());
    return;
  }
  cache_storage.value()->Open(
      cache_name, trace_id,
      WTF::BindOnce(
          [](String request, int64_t trace_id,
             scoped_refptr<RequestCallbackWrapper<DeleteEntryCallback>>
                 callback_wrapper,
             String cache_name, mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback_wrapper->SendFailure(ProtocolResponse::ServerError(
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
                         scoped_refptr<RequestCallbackWrapper<
                             DeleteEntryCallback>> callback_wrapper,
                         mojom::blink::CacheStorageVerboseErrorPtr error) {
                        if (error->value !=
                            mojom::blink::CacheStorageError::kSuccess) {
                          callback_wrapper->SendFailure(
                              ProtocolResponse::ServerError(
                                  String::Format(
                                      "Error deleting cache entry: %s",
                                      CacheStorageErrorString(error->value))
                                      .Utf8()));
                        } else {
                          callback_wrapper->SendSuccess();
                        }
                      },
                      std::move(cache_remote), std::move(callback_wrapper)));
            }
          },
          request, trace_id, std::move(callback_wrapper), cache_name));
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

  auto callback_wrapper =
      RequestCallbackWrapper<RequestCachedResponseCallback>::Wrap(
          std::move(callback));

  String cache_name;
  auto cache_storage = GetCacheStorageRemoteForId(
      cache_id, cache_name, callback_wrapper->GetFailureCallback());
  if (!cache_storage.has_value()) {
    callback_wrapper->SendFailure(cache_storage.error());
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

  cache_storage.value()->Match(
      std::move(request), std::move(multi_query_options),
      /*in_related_fetch_event=*/false,
      /*in_range_fetch_event=*/false, trace_id,
      WTF::BindOnce(
          [](scoped_refptr<RequestCallbackWrapper<
                 RequestCachedResponseCallback>> callback_wrapper,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             mojom::blink::MatchResultPtr result) {
            if (result->is_status()) {
              callback_wrapper->SendFailure(ProtocolResponse::ServerError(
                  String::Format("Unable to read cached response: %s",
                                 CacheStorageErrorString(result->get_status()))
                      .Utf8()));
            } else {
              std::unique_ptr<protocol::DictionaryValue> headers =
                  protocol::DictionaryValue::create();
              if (!result->get_response()->blob) {
                callback_wrapper->SendSuccess(CachedResponse::create()
                                                  .setBody(protocol::Binary())
                                                  .build());
                return;
              }
              CachedResponseFileReaderLoaderClient::Load(
                  std::move(task_runner),
                  std::move(result->get_response()->blob), callback_wrapper);
            }
          },
          std::move(callback_wrapper), std::move(task_runner)));
}
}  // namespace blink
