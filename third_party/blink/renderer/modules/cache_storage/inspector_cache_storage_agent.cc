// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/inspector_cache_storage_agent.h"

#include <algorithm>
#include <memory>
#include <utility>
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
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

String BuildCacheId(const String& security_origin, const String& cache_name) {
  String id(security_origin);
  id.append('|');
  id.append(cache_name);
  return id;
}

ProtocolResponse ParseCacheId(const String& id,
                              String* security_origin,
                              String* cache_name) {
  wtf_size_t pipe = id.find('|');
  if (pipe == WTF::kNotFound)
    return ProtocolResponse::Error("Invalid cache id.");
  *security_origin = id.Substring(0, pipe);
  *cache_name = id.Substring(pipe + 1);
  return ProtocolResponse::OK();
}

ProtocolResponse GetExecutionContext(InspectedFrames* frames,
                                     const String& security_origin,
                                     ExecutionContext** context) {
  LocalFrame* frame = frames->FrameWithSecurityOrigin(security_origin);
  if (!frame)
    return ProtocolResponse::Error("No frame with origin " + security_origin);

  blink::Document* document = frame->GetDocument();
  if (!document)
    return ProtocolResponse::Error("No execution context found");

  *context = document;

  return ProtocolResponse::OK();
}

ProtocolResponse AssertCacheStorage(
    const String& security_origin,
    InspectedFrames* frames,
    InspectorCacheStorageAgent::CachesMap* caches,
    mojom::blink::CacheStorage** result) {
  scoped_refptr<const SecurityOrigin> sec_origin =
      SecurityOrigin::CreateFromString(security_origin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!sec_origin->IsPotentiallyTrustworthy()) {
    return ProtocolResponse::Error(
        sec_origin->IsPotentiallyTrustworthyErrorMessage());
  }

  ExecutionContext* context = nullptr;
  ProtocolResponse response =
      GetExecutionContext(frames, security_origin, &context);
  if (!response.isSuccess())
    return response;

  auto it = caches->find(security_origin);

  if (it == caches->end()) {
    mojom::blink::CacheStoragePtr cache_storage_ptr;
    context->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&cache_storage_ptr));
    *result = cache_storage_ptr.get();
    caches->Set(security_origin, std::move(cache_storage_ptr));
  } else {
    *result = it->value.get();
  }

  return ProtocolResponse::OK();
}

ProtocolResponse AssertCacheStorageAndNameForId(
    const String& cache_id,
    InspectedFrames* frames,
    String* cache_name,
    InspectorCacheStorageAgent::CachesMap* caches,
    mojom::blink::CacheStorage** result) {
  String security_origin;
  ProtocolResponse response =
      ParseCacheId(cache_id, &security_origin, cache_name);
  if (!response.isSuccess())
    return response;
  return AssertCacheStorage(security_origin, frames, caches, result);
}

CString CacheStorageErrorString(mojom::blink::CacheStorageError error) {
  switch (error) {
    case mojom::blink::CacheStorageError::kErrorNotImplemented:
      return CString("not implemented.");
    case mojom::blink::CacheStorageError::kErrorNotFound:
      return CString("not found.");
    case mojom::blink::CacheStorageError::kErrorExists:
      return CString("cache already exists.");
    case mojom::blink::CacheStorageError::kErrorQuotaExceeded:
      return CString("quota exceeded.");
    case mojom::blink::CacheStorageError::kErrorCacheNameNotFound:
      return CString("cache not found.");
    case mojom::blink::CacheStorageError::kErrorQueryTooLarge:
      return CString("operation too large.");
    case mojom::blink::CacheStorageError::kErrorStorage:
      return CString("storage failure.");
    case mojom::blink::CacheStorageError::kErrorDuplicateOperation:
      return CString("duplicate operation.");
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
    case network::mojom::FetchResponseType::kCORS:
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
  int page_size;
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
  ResponsesAccumulator(wtf_size_t num_responses,
                       const DataRequestParams& params,
                       mojom::blink::CacheStorageCacheAssociatedPtr cache_ptr,
                       std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params),
        num_responses_left_(num_responses),
        responses_(num_responses),
        cache_ptr_(std::move(cache_ptr)),
        callback_(std::move(callback)) {}

  void Dispatch(const Vector<WebServiceWorkerRequest>& requests) {
    for (const auto& request : requests) {
      cache_ptr_->Match(
          request, mojom::blink::QueryParams::New(),
          WTF::Bind(
              [](scoped_refptr<ResponsesAccumulator> accumulator,
                 WebServiceWorkerRequest request,
                 mojom::blink::MatchResultPtr result) {
                if (result->is_status()) {
                  accumulator->SendFailure(result->get_status());
                } else {
                  accumulator->AddRequestResponsePair(request,
                                                      result->get_response());
                }
              },
              scoped_refptr<ResponsesAccumulator>(this), request));
    }
  }

  void AddRequestResponsePair(
      const WebServiceWorkerRequest& request,
      const mojom::blink::FetchAPIResponsePtr& response) {
    DCHECK_GT(num_responses_left_, 0);
    RequestResponse& request_response =
        responses_.at(responses_.size() - num_responses_left_);

    request_response.request_url = request.Url().GetString();
    request_response.request_method = request.Method();
    request_response.request_headers = request.Headers();
    request_response.response_status = response->status_code;
    request_response.response_status_text = response->status_text;
    request_response.response_time = response->response_time.ToDoubleT();
    request_response.response_type = response->response_type;
    for (const auto& header : response->headers) {
      request_response.response_headers.Set(AtomicString(header.key),
                                            AtomicString(header.value));
    }

    if (--num_responses_left_ != 0)
      return;

    std::sort(responses_.begin(), responses_.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::CodePointCompareLessThan(a.request_url,
                                                     b.request_url);
              });
    if (params_.skip_count > 0)
      responses_.EraseAt(0, params_.skip_count);
    bool has_more = false;
    if (static_cast<size_t>(params_.page_size) < responses_.size()) {
      responses_.EraseAt(params_.page_size,
                         responses_.size() - params_.page_size);
      has_more = true;
    }
    std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
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
      array->addItem(std::move(entry));
    }
    callback_->sendSuccess(std::move(array), has_more);
  }

  void SendFailure(const mojom::blink::CacheStorageError& error) {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Error requesting responses for cache %s : %s",
                       params_.cache_name.Utf8().data(),
                       CacheStorageErrorString(error).data())));
  }

  std::unique_ptr<Array<Header>> SerializeHeaders(
      const HTTPHeaderMap& headers) {
    std::unique_ptr<Array<Header>> result = Array<Header>::create();
    for (HTTPHeaderMap::const_iterator it = headers.begin(),
                                       end = headers.end();
         it != end; ++it) {
      result->addItem(
          Header::create().setName(it->key).setValue(it->value).build());
    }
    return result;
  }

 private:
  DataRequestParams params_;
  int num_responses_left_;
  Vector<RequestResponse> responses_;
  mojom::blink::CacheStorageCacheAssociatedPtr cache_ptr_;
  std::unique_ptr<RequestEntriesCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(ResponsesAccumulator);
};

class GetCacheKeysForRequestData {
 public:
  GetCacheKeysForRequestData(
      const DataRequestParams& params,
      mojom::blink::CacheStorageCacheAssociatedPtrInfo cache_ptr_info,
      std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params), callback_(std::move(callback)) {
    cache_ptr_.Bind(std::move(cache_ptr_info));
  }

  void Dispatch(std::unique_ptr<GetCacheKeysForRequestData> self) {
    cache_ptr_->Keys(
        base::Optional<WebServiceWorkerRequest>(),
        mojom::blink::QueryParams::New(),
        WTF::Bind(
            [](DataRequestParams params,
               std::unique_ptr<GetCacheKeysForRequestData> self,
               mojom::blink::CacheKeysResultPtr result) {
              if (result->is_status()) {
                self->callback_->sendFailure(
                    ProtocolResponse::Error(String::Format(
                        "Error requesting requests for cache %s: %s",
                        params.cache_name.Utf8().data(),
                        CacheStorageErrorString(result->get_status()).data())));
              } else {
                auto& requests = result->get_keys();
                if (requests.IsEmpty()) {
                  std::unique_ptr<Array<DataEntry>> array =
                      Array<DataEntry>::create();
                  self->callback_->sendSuccess(std::move(array), false);
                  return;
                }
                scoped_refptr<ResponsesAccumulator> accumulator =
                    base::AdoptRef(new ResponsesAccumulator(
                        requests.size(), params, std::move(self->cache_ptr_),
                        std::move(self->callback_)));
                accumulator->Dispatch(result->get_keys());
              }
            },
            params_, std::move(self)));
  }

 private:
  DataRequestParams params_;
  mojom::blink::CacheStorageCacheAssociatedPtr cache_ptr_;
  std::unique_ptr<RequestEntriesCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(GetCacheKeysForRequestData);
};

class CachedResponseFileReaderLoaderClient final
    : private FileReaderLoaderClient {
 public:
  static void Load(scoped_refptr<BlobDataHandle> blob,
                   std::unique_ptr<RequestCachedResponseCallback> callback) {
    new CachedResponseFileReaderLoaderClient(std::move(blob),
                                             std::move(callback));
  }

  void DidStartLoading() override {}

  void DidFinishLoading() override {
    std::unique_ptr<CachedResponse> response =
        CachedResponse::create()
            .setBody(
                Base64Encode(data_->Data(), SafeCast<unsigned>(data_->size())))
            .build();
    callback_->sendSuccess(std::move(response));
    dispose();
  }

  void DidFail(FileError::ErrorCode error) override {
    callback_->sendFailure(ProtocolResponse::Error(String::Format(
        "Unable to read the cached response, error code: %d", error)));
    dispose();
  }

  void DidReceiveDataForClient(const char* data,
                               unsigned data_length) override {
    data_->Append(data, data_length);
  }

 private:
  CachedResponseFileReaderLoaderClient(
      scoped_refptr<BlobDataHandle>&& blob,
      std::unique_ptr<RequestCachedResponseCallback>&& callback)
      : loader_(
            FileReaderLoader::Create(FileReaderLoader::kReadByClient, this)),
        callback_(std::move(callback)),
        data_(SharedBuffer::Create()) {
    loader_->Start(std::move(blob));
  }

  ~CachedResponseFileReaderLoaderClient() override = default;

  void dispose() { delete this; }

  std::unique_ptr<FileReaderLoader> loader_;
  std::unique_ptr<RequestCachedResponseCallback> callback_;
  scoped_refptr<SharedBuffer> data_;

  DISALLOW_COPY_AND_ASSIGN(CachedResponseFileReaderLoaderClient);
};

}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent(InspectedFrames* frames)
    : frames_(frames) {}

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

void InspectorCacheStorageAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorCacheStorageAgent::requestCacheNames(
    const String& security_origin,
    std::unique_ptr<RequestCacheNamesCallback> callback) {
  scoped_refptr<const SecurityOrigin> sec_origin =
      SecurityOrigin::CreateFromString(security_origin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!sec_origin->IsPotentiallyTrustworthy()) {
    // Don't treat this as an error, just don't attempt to open and enumerate
    // the caches.
    callback->sendSuccess(Array<ProtocolCache>::create());
    return;
  }

  mojom::blink::CacheStorage* cache_storage = nullptr;

  ProtocolResponse response =
      AssertCacheStorage(security_origin, frames_, &caches_, &cache_storage);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }

  cache_storage->Keys(WTF::Bind(
      [](String security_origin,
         std::unique_ptr<RequestCacheNamesCallback> callback,
         const Vector<String>& caches) {
        std::unique_ptr<Array<ProtocolCache>> array =
            Array<ProtocolCache>::create();
        for (auto& cache : caches) {
          array->addItem(ProtocolCache::create()
                             .setSecurityOrigin(security_origin)
                             .setCacheName(cache)
                             .setCacheId(BuildCacheId(security_origin, cache))
                             .build());
        }
        callback->sendSuccess(std::move(array));
      },
      security_origin, std::move(callback)));
}

void InspectorCacheStorageAgent::requestEntries(
    const String& cache_id,
    int skip_count,
    int page_size,
    std::unique_ptr<RequestEntriesCallback> callback) {
  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  DataRequestParams params;
  params.cache_name = cache_name;
  params.page_size = page_size;
  params.skip_count = skip_count;

  cache_storage->Open(
      cache_name,
      WTF::Bind(
          [](DataRequestParams params,
             std::unique_ptr<RequestEntriesCallback> callback,
             mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::Error(String::Format(
                  "Error requesting cache %s: %s",
                  params.cache_name.Utf8().data(),
                  CacheStorageErrorString(result->get_status()).data())));
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
  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache_storage->Delete(
      cache_name,
      WTF::Bind(
          [](std::unique_ptr<DeleteCacheCallback> callback,
             mojom::blink::CacheStorageError error) {
            if (error == mojom::blink::CacheStorageError::kSuccess) {
              callback->sendSuccess();
            } else {
              callback->sendFailure(ProtocolResponse::Error(
                  String::Format("Error requesting cache names: %s",
                                 CacheStorageErrorString(error).data())));
            }
          },
          std::move(callback)));
}

void InspectorCacheStorageAgent::deleteEntry(
    const String& cache_id,
    const String& request,
    std::unique_ptr<DeleteEntryCallback> callback) {
  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache_storage->Open(
      cache_name,
      WTF::Bind(
          [](String cache_name, String request,
             std::unique_ptr<DeleteEntryCallback> callback,
             mojom::blink::OpenResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::Error(String::Format(
                  "Error requesting cache %s: %s", cache_name.Utf8().data(),
                  CacheStorageErrorString(result->get_status()).data())));
            } else {
              Vector<mojom::blink::BatchOperationPtr> batch_operations;
              batch_operations.push_back(mojom::blink::BatchOperation::New());
              auto& operation = batch_operations.back();
              operation->operation_type = mojom::blink::OperationType::kDelete;
              operation->request.SetURL(KURL(request));
              operation->request.SetMethod("GET");

              mojom::blink::CacheStorageCacheAssociatedPtr cache_ptr;
              cache_ptr.Bind(std::move(result->get_cache()));
              auto* cache = cache_ptr.get();
              cache->Batch(
                  std::move(batch_operations), true /* fail_on_duplicates */,
                  WTF::Bind(
                      [](mojom::blink::CacheStorageCacheAssociatedPtr cache_ptr,
                         std::unique_ptr<DeleteEntryCallback> callback,
                         mojom::blink::CacheStorageVerboseErrorPtr error) {
                        if (error->value !=
                            mojom::blink::CacheStorageError::kSuccess) {
                          callback->sendFailure(
                              ProtocolResponse::Error(String::Format(
                                  "Error deleting cache entry: %s",
                                  CacheStorageErrorString(error->value)
                                      .data())));
                        } else {
                          callback->sendSuccess();
                        }
                      },
                      std::move(cache_ptr), std::move(callback)));
            }
          },
          cache_name, request, std::move(callback)));
}

void InspectorCacheStorageAgent::requestCachedResponse(
    const String& cache_id,
    const String& request_url,
    std::unique_ptr<RequestCachedResponseCallback> callback) {
  String cache_name;
  mojom::blink::CacheStorage* cache_storage = nullptr;
  ProtocolResponse response = AssertCacheStorageAndNameForId(
      cache_id, frames_, &cache_name, &caches_, &cache_storage);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  WebServiceWorkerRequest request;
  request.SetURL(KURL(request_url));
  request.SetMethod("GET");
  cache_storage->Match(
      request, mojom::blink::QueryParams::New(),
      WTF::Bind(
          [](std::unique_ptr<RequestCachedResponseCallback> callback,
             mojom::blink::MatchResultPtr result) {
            if (result->is_status()) {
              callback->sendFailure(ProtocolResponse::Error(String::Format(
                  "Unable to read cached response: %s",
                  CacheStorageErrorString(result->get_status()).data())));
            } else {
              std::unique_ptr<protocol::DictionaryValue> headers =
                  protocol::DictionaryValue::create();
              if (!result->get_response()->blob) {
                callback->sendSuccess(
                    CachedResponse::create().setBody("").build());
                return;
              }
              CachedResponseFileReaderLoaderClient::Load(
                  std::move(result->get_response()->blob), std::move(callback));
            }
          },
          std::move(callback)));
}
}  // namespace blink
