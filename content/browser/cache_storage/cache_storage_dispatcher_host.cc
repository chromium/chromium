// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_trace_utils.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;
using network::CrossOriginEmbedderPolicy;
using network::CrossOriginResourcePolicy;
using network::mojom::FetchResponseType;
using network::mojom::RequestMode;

// TODO(lucmult): Check this before binding.
bool OriginCanAccessCacheStorage(const url::Origin& origin) {
  return !origin.opaque() &&
         blink::network_utils::IsOriginSecure(origin.GetURL());
}

// Verifies that the BatchOperation list conforms to the constraints imposed
// by the web exposed Cache API.  Don't permit compromised renderers to use
// unexpected operation combinations.
bool ValidBatchOperations(
    const std::vector<blink::mojom::BatchOperationPtr>& batch_operations) {
  // At least one operation is required.
  if (batch_operations.empty())
    return false;
  blink::mojom::OperationType type = batch_operations[0]->operation_type;
  // We must have a defined operation type.  All other enum values allowed
  // by the mojo validator are permitted here.
  if (type == blink::mojom::OperationType::kUndefined)
    return false;
  // Delete operations should only be sent one at a time.
  if (type == blink::mojom::OperationType::kDelete &&
      batch_operations.size() > 1) {
    return false;
  }
  // All operations in the list must be the same.
  for (const auto& op : batch_operations) {
    if (op->operation_type != type)
      return false;
  }
  return true;
}

blink::mojom::MatchResultPtr EagerlyReadResponseBody(
    blink::mojom::FetchAPIResponsePtr response) {
  if (!response->blob)
    return blink::mojom::MatchResult::NewResponse(std::move(response));

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(response->blob->size);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv = CreateDataPipe(&options, &producer_handle, &consumer_handle);
  if (rv != MOJO_RESULT_OK)
    return blink::mojom::MatchResult::NewResponse(std::move(response));

  mojo::PendingRemote<blink::mojom::BlobReaderClient> reader_client;
  auto pending_receiver = reader_client.InitWithNewPipeAndPassReceiver();

  mojo::Remote<blink::mojom::Blob> blob(std::move(response->blob->blob));
  blob->ReadAll(std::move(producer_handle), std::move(reader_client));

  // Clear the main body blob entry.  There should still be a |side_data_blob|
  // value for reading code cache, however.
  response->blob = nullptr;
  DCHECK(response->side_data_blob);

  return blink::mojom::MatchResult::NewEagerResponse(
      blink::mojom::EagerResponse::New(std::move(response),
                                       std::move(consumer_handle),
                                       std::move(pending_receiver)));
}

// Enforce the Cross-Origin-Resource-Policy (CORP) of the response
// against the requesting document's origin and
// Cross-Origin-Embedder-Policy (COEP).
// See https://github.com/w3c/ServiceWorker/issues/1490.
bool ResponseBlockedByCrossOriginResourcePolicy(
    const blink::mojom::FetchAPIResponse* response,
    const url::Origin& document_origin,
    const CrossOriginEmbedderPolicy& document_coep,
    const mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>&
        coep_reporter) {
  // optional short-circuit to avoid parsing CORP again and again when no COEP
  // policy is defined.
  if (document_coep.value ==
          network::mojom::CrossOriginEmbedderPolicyValue::kNone &&
      document_coep.report_only_value ==
          network::mojom::CrossOriginEmbedderPolicyValue::kNone) {
    return false;
  }

  // Cross-Origin-Resource-Policy is checked only for cross-origin responses
  // that were requested by no-cors requests. Those result in opaque responses.
  // See https://github.com/whatwg/fetch/issues/985.
  if (response->response_type != FetchResponseType::kOpaque)
    return false;

  base::Optional<std::string> corp_header_value;
  auto corp_header =
      response->headers.find(network::CrossOriginResourcePolicy::kHeaderName);
  if (corp_header != response->headers.end())
    corp_header_value = corp_header->second;

  return CrossOriginResourcePolicy::IsBlockedByHeaderValue(
             response->url_list.back(), response->url_list.front(),
             document_origin, corp_header_value, RequestMode::kNoCors,
             document_origin, network::mojom::RequestDestination::kEmpty,
             document_coep, coep_reporter ? coep_reporter.get() : nullptr)
      .has_value();
}

}  // namespace

// Implements the mojom interface CacheStorageCache. It's owned by
// CacheStorageDispatcherHost and it's destroyed when client drops the mojo
// remote which in turn removes from UniqueAssociatedReceiverSet in
// CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheImpl
    : public blink::mojom::CacheStorageCache {
 public:
  explicit CacheImpl(
      CacheStorageCacheHandle cache_handle,
      const url::Origin& origin,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter)
      : cache_handle_(std::move(cache_handle)),
        origin_(origin),
        cross_origin_embedder_policy_(cross_origin_embedder_policy),
        coep_reporter_(std::move(coep_reporter)) {}

  ~CacheImpl() override { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // blink::mojom::CacheStorageCache implementation:
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::CacheQueryOptionsPtr match_options,
             bool in_related_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW2("CacheStorage",
                           "CacheStorageDispatchHost::CacheImpl::Match",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "request", CacheStorageTracedValue(request),
                           "options", CacheStorageTracedValue(match_options));

    content::CacheStorageCache* cache = cache_handle_.value();
    bool cache_initialized =
        cache ? cache->GetInitState() ==
                    content::CacheStorageCache::InitState::Initialized
              : false;

    auto cb = base::BindOnce(
        [](base::WeakPtr<CacheImpl> self, base::TimeTicks start_time,
           bool ignore_search, bool in_related_fetch_event,
           bool cache_initialized, int64_t trace_id,
           blink::mojom::CacheStorageCache::MatchCallback callback,
           blink::mojom::CacheStorageError error,
           blink::mojom::FetchAPIResponsePtr response) {
          if (!self)
            return;
          base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
          UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.Match",
                                   elapsed);
          if (ignore_search) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Browser.Match.IgnoreSearch", elapsed);
          }
          if (cache_initialized) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Browser.Match.Initialized", elapsed);
          }
          if (in_related_fetch_event) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Browser.Match.RelatedFetchEvent",
                elapsed);
          }
          if (error == CacheStorageError::kErrorNotFound) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Browser.Match.Miss", elapsed);
          }
          if (error != CacheStorageError::kSuccess) {
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage",
                "CacheStorageDispatchHost::CacheImpl::Match::Callback",
                TRACE_ID_GLOBAL(trace_id),
                TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
                CacheStorageTracedValue(error));
            std::move(callback).Run(
                blink::mojom::MatchResult::NewStatus(error));
            return;
          }

          // Enforce the Cross-Origin-Resource-Policy (CORP) of the response
          // against the requesting document's origin and
          // Cross-Origin-Embedder-Policy (COEP).
          if (ResponseBlockedByCrossOriginResourcePolicy(
                  response.get(), self->origin_,
                  self->cross_origin_embedder_policy_, self->coep_reporter_)) {
            std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
                CacheStorageError::kErrorCrossOriginResourcePolicy));
            return;
          }

          UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.Match.Hit",
                                   elapsed);
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheImpl::Match::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "response",
              CacheStorageTracedValue(response));

          blink::mojom::MatchResultPtr result;
          if (in_related_fetch_event) {
            result = EagerlyReadResponseBody(std::move(response));
          } else {
            result =
                blink::mojom::MatchResult::NewResponse(std::move(response));
          }
          std::move(callback).Run(std::move(result));
        },
        weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
        match_options->ignore_search, in_related_fetch_event, cache_initialized,
        trace_id, std::move(callback));

    if (!cache) {
      std::move(cb).Run(CacheStorageError::kErrorNotFound, nullptr);
      return;
    }

    CacheStorageSchedulerPriority priority =
        CacheStorageSchedulerPriority::kNormal;
    if (in_related_fetch_event)
      priority = CacheStorageSchedulerPriority::kHigh;

    cache->Match(std::move(request), std::move(match_options), priority,
                 trace_id, std::move(cb));
  }

  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr match_options,
                int64_t trace_id,
                MatchAllCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW2("CacheStorage",
                           "CacheStorageDispatchHost::CacheImpl::MatchAll",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "request", CacheStorageTracedValue(request),
                           "options", CacheStorageTracedValue(match_options));

    auto cb = base::BindOnce(
        [](base::WeakPtr<CacheImpl> self, base::TimeTicks start_time,
           int64_t trace_id,
           blink::mojom::CacheStorageCache::MatchAllCallback callback,
           blink::mojom::CacheStorageError error,
           std::vector<blink::mojom::FetchAPIResponsePtr> responses) {
          if (!self)
            return;
          base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
          UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.MatchAll",
                                   elapsed);
          if (error != CacheStorageError::kSuccess &&
              error != CacheStorageError::kErrorNotFound) {
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage",
                "CacheStorageDispatchHost::CacheImpl::MatchAll::Callback",
                TRACE_ID_GLOBAL(trace_id),
                TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
                CacheStorageTracedValue(error));
            std::move(callback).Run(
                blink::mojom::MatchAllResult::NewStatus(error));
            return;
          }

          // Enforce the Cross-Origin-Resource-Policy (CORP) of the response
          // against the requesting document's origin and
          // Cross-Origin-Embedder-Policy (COEP).
          for (const auto& response : responses) {
            if (ResponseBlockedByCrossOriginResourcePolicy(
                    response.get(), self->origin_,
                    self->cross_origin_embedder_policy_,
                    self->coep_reporter_)) {
              std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(
                  CacheStorageError::kErrorCrossOriginResourcePolicy));
              return;
            }
          }

          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheImpl::MatchAll::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
              "response_list", CacheStorageTracedValue(responses));
          std::move(callback).Run(
              blink::mojom::MatchAllResult::NewResponses(std::move(responses)));
        },
        weak_factory_.GetWeakPtr(), base::TimeTicks::Now(), trace_id,
        std::move(callback));

    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(cb).Run(CacheStorageError::kErrorNotFound,
                        std::vector<blink::mojom::FetchAPIResponsePtr>());
      return;
    }

    cache->MatchAll(std::move(request), std::move(match_options), trace_id,
                    std::move(cb));
  }

  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::CacheQueryOptionsPtr match_options,
            int64_t trace_id,
            KeysCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW2("CacheStorage",
                           "CacheStorageDispatchHost::CacheImpl::Keys",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "request", CacheStorageTracedValue(request),
                           "options", CacheStorageTracedValue(match_options));

    auto cb = base::BindOnce(
        [](base::TimeTicks start_time, int64_t trace_id,
           blink::mojom::CacheStorageCache::KeysCallback callback,
           blink::mojom::CacheStorageError error,
           std::unique_ptr<content::CacheStorageCache::Requests> requests) {
          UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.Keys",
                                   base::TimeTicks::Now() - start_time);
          if (error != CacheStorageError::kSuccess) {
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage",
                "CacheStorageDispatchHost::CacheImpl::Keys::Callback",
                TRACE_ID_GLOBAL(trace_id),
                TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
                CacheStorageTracedValue(error));
            std::move(callback).Run(
                blink::mojom::CacheKeysResult::NewStatus(error));
            return;
          }
          std::vector<blink::mojom::FetchAPIRequestPtr> requests_;
          for (const auto& request : *requests) {
            requests_.push_back(
                BackgroundFetchSettledFetch::CloneRequest(request));
          }

          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheImpl::Keys::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
              "request_list", CacheStorageTracedValue(requests_));

          std::move(callback).Run(
              blink::mojom::CacheKeysResult::NewKeys(std::move(requests_)));
        },
        base::TimeTicks::Now(), trace_id, std::move(callback));

    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(cb).Run(CacheStorageError::kErrorNotFound, nullptr);
      return;
    }

    cache->Keys(std::move(request), std::move(match_options), trace_id,
                std::move(cb));
  }

  void Batch(std::vector<blink::mojom::BatchOperationPtr> batch_operations,
             int64_t trace_id,
             BatchCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW1(
        "CacheStorage", "CacheStorageDispatchHost::CacheImpl::Batch",
        TRACE_ID_GLOBAL(trace_id),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "operation_list",
        CacheStorageTracedValue(batch_operations));

    if (!ValidBatchOperations(batch_operations)) {
      mojo::ReportBadMessage("CSDH_UNEXPECTED_OPERATION");
      return;
    }

    // Validated batch operations always have at least one entry.
    blink::mojom::OperationType operation_type =
        batch_operations[0]->operation_type;
    int operation_count = batch_operations.size();

    auto cb = base::BindOnce(
        [](base::TimeTicks start_time,
           blink::mojom::OperationType operation_type, int operation_count,
           int64_t trace_id,
           blink::mojom::CacheStorageCache::BatchCallback callback,
           blink::mojom::CacheStorageVerboseErrorPtr error) {
          base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheImpl::Batch::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
              CacheStorageTracedValue(error->value));
          if (operation_type == blink::mojom::OperationType::kDelete) {
            DCHECK_EQ(operation_count, 1);
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Browser.DeleteOne", elapsed);
          } else if (operation_count > 1) {
            DCHECK_EQ(operation_type, blink::mojom::OperationType::kPut);
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.PutMany",
                                     elapsed);
          } else {
            DCHECK_EQ(operation_type, blink::mojom::OperationType::kPut);
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Browser.PutOne",
                                     elapsed);
          }
          std::move(callback).Run(std::move(error));
        },
        base::TimeTicks::Now(), operation_type, operation_count, trace_id,
        std::move(callback));

    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(cb).Run(CacheStorageVerboseError::New(
          CacheStorageError::kErrorNotFound, base::nullopt));
      return;
    }

    cache->BatchOperation(
        std::move(batch_operations), trace_id, std::move(cb),
        base::BindOnce(
            [](mojo::ReportBadMessageCallback bad_message_callback) {
              std::move(bad_message_callback).Run("CSDH_UNEXPECTED_OPERATION");
            },
            mojo::GetBadMessageCallback()));
  }

  CacheStorageCacheHandle cache_handle_;
  const url::Origin origin_;
  const CrossOriginEmbedderPolicy cross_origin_embedder_policy_;
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CacheImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CacheImpl);
};

// Implements the mojom interface CacheStorage. It's owned by the
// CacheStorageDispatcherHost.  The CacheStorageImpl is destroyed when the
// client drops its mojo remote which in turn removes from UniqueReceiverSet in
// CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheStorageImpl final
    : public blink::mojom::CacheStorage {
 public:
  CacheStorageImpl(
      CacheStorageDispatcherHost* owner,
      const url::Origin& origin,
      bool incognito,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter)
      : owner_(owner),
        origin_(origin),
        cross_origin_embedder_policy_(cross_origin_embedder_policy),
        coep_reporter_(std::move(coep_reporter)) {
    // Eagerly initialize the backend when the mojo connection is bound.
    //
    // Note, we only do this for non-incognito mode.  The memory cache mode
    // will incorrectly report cache file usage and break tests if we eagerly
    // initialize it here.  Also, eagerly initializing memory cache mode does
    // not really provide any performance benefit.
    if (!incognito) {
      content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
      if (cache_storage) {
        cache_storage->Init();
      }
    }
  }

  ~CacheStorageImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Mojo CacheStorage Interface implementation:
  void Keys(int64_t trace_id,
            blink::mojom::CacheStorage::KeysCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW0(
        "CacheStorage", "CacheStorageDispatchHost::CacheStorageImpl::Keys",
        TRACE_ID_GLOBAL(trace_id),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

    auto cb = base::BindOnce(
        [](base::TimeTicks start_time, int64_t trace_id,
           blink::mojom::CacheStorage::KeysCallback callback,
           std::vector<std::string> cache_names) {
          std::vector<base::string16> string16s;
          for (const auto& name : cache_names) {
            string16s.push_back(base::UTF8ToUTF16(name));
          }
          UMA_HISTOGRAM_LONG_TIMES(
              "ServiceWorkerCache.CacheStorage.Browser.Keys",
              base::TimeTicks::Now() - start_time);
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheStorageImpl::Keys::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "key_list",
              CacheStorageTracedValue(string16s));
          std::move(callback).Run(string16s);
        },
        base::TimeTicks::Now(), trace_id, std::move(callback));

    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(cb).Run(std::vector<std::string>());
      return;
    }

    cache_storage->EnumerateCaches(trace_id, std::move(cb));
  }

  void Delete(const base::string16& cache_name,
              int64_t trace_id,
              blink::mojom::CacheStorage::DeleteCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::string utf8_cache_name = base::UTF16ToUTF8(cache_name);
    TRACE_EVENT_WITH_FLOW1("CacheStorage",
                           "CacheStorageDispatchHost::CacheStorageImpl::Delete",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "cache_name", utf8_cache_name);

    auto cb = base::BindOnce(
        [](base::TimeTicks start_time, int64_t trace_id,
           blink::mojom::CacheStorage::DeleteCallback callback,
           CacheStorageError error) {
          UMA_HISTOGRAM_LONG_TIMES(
              "ServiceWorkerCache.CacheStorage.Browser.Delete",
              base::TimeTicks::Now() - start_time);
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheStorageImpl::Delete::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
              CacheStorageTracedValue(error));
          std::move(callback).Run(error);
        },
        base::TimeTicks::Now(), trace_id, std::move(callback));

    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(cb).Run(MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }

    cache_storage->DoomCache(utf8_cache_name, trace_id, std::move(cb));
  }

  void Has(const base::string16& cache_name,
           int64_t trace_id,
           blink::mojom::CacheStorage::HasCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::string utf8_cache_name = base::UTF16ToUTF8(cache_name);
    TRACE_EVENT_WITH_FLOW1("CacheStorage",
                           "CacheStorageDispatchHost::CacheStorageImpl::Has",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "cache_name", utf8_cache_name);

    auto cb = base::BindOnce(
        [](base::TimeTicks start_time, int64_t trace_id,
           blink::mojom::CacheStorage::HasCallback callback, bool has_cache,
           CacheStorageError error) {
          if (!has_cache)
            error = CacheStorageError::kErrorNotFound;
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheStorageImpl::Has::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
              CacheStorageTracedValue(error));
          UMA_HISTOGRAM_LONG_TIMES(
              "ServiceWorkerCache.CacheStorage.Browser.Has",
              base::TimeTicks::Now() - start_time);
          std::move(callback).Run(error);
        },
        base::TimeTicks::Now(), trace_id, std::move(callback));

    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(cb).Run(/* has_cache = */ false,
                        MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }

    cache_storage->HasCache(utf8_cache_name, trace_id, std::move(cb));
  }

  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::MultiCacheQueryOptionsPtr match_options,
             bool in_related_fetch_event,
             int64_t trace_id,
             blink::mojom::CacheStorage::MatchCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT_WITH_FLOW2("CacheStorage",
                           "CacheStorageDispatchHost::CacheStorageImpl::Match",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "request", CacheStorageTracedValue(request),
                           "options", CacheStorageTracedValue(match_options));

    auto cb = BindOnce(
        [](base::WeakPtr<CacheStorageImpl> self, base::TimeTicks start_time,
           bool match_all_caches, bool in_related_fetch_event, int64_t trace_id,
           blink::mojom::CacheStorage::MatchCallback callback,
           CacheStorageError error,
           blink::mojom::FetchAPIResponsePtr response) {
          if (!self)
            return;

          base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
          if (match_all_caches) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.CacheStorage.Browser.MatchAllCaches",
                elapsed);
          } else {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.CacheStorage.Browser.MatchOneCache",
                elapsed);
          }
          if (error != CacheStorageError::kSuccess) {
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage",
                "CacheStorageDispatchHost::CacheStorageImpl::Match::Callback",
                TRACE_ID_GLOBAL(trace_id),
                TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
                CacheStorageTracedValue(error));
            std::move(callback).Run(
                blink::mojom::MatchResult::NewStatus(error));
            return;
          }
          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheStorageImpl::Match::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "response",
              CacheStorageTracedValue(response));

          // Enforce the Cross-Origin-Resource-Policy (CORP) of the response
          // against the requesting document's origin and
          // Cross-Origin-Embedder-Policy (COEP).
          if (ResponseBlockedByCrossOriginResourcePolicy(
                  response.get(), self->origin_,
                  self->cross_origin_embedder_policy_, self->coep_reporter_)) {
            std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
                CacheStorageError::kErrorCrossOriginResourcePolicy));
            return;
          }

          blink::mojom::MatchResultPtr result;
          if (in_related_fetch_event) {
            result = EagerlyReadResponseBody(std::move(response));
          } else {
            result =
                blink::mojom::MatchResult::NewResponse(std::move(response));
          }
          std::move(callback).Run(std::move(result));
        },
        weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
        !match_options->cache_name, in_related_fetch_event, trace_id,
        std::move(callback));

    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(cb).Run(CacheStorageError::kErrorNotFound, nullptr);
      return;
    }

    CacheStorageSchedulerPriority priority =
        CacheStorageSchedulerPriority::kNormal;
    if (in_related_fetch_event)
      priority = CacheStorageSchedulerPriority::kHigh;

    if (!match_options->cache_name) {
      cache_storage->MatchAllCaches(std::move(request),
                                    std::move(match_options->query_options),
                                    priority, trace_id, std::move(cb));
      return;
    }
    std::string cache_name = base::UTF16ToUTF8(*match_options->cache_name);
    cache_storage->MatchCache(std::move(cache_name), std::move(request),
                              std::move(match_options->query_options), priority,
                              trace_id, std::move(cb));
  }

  void Open(const base::string16& cache_name,
            int64_t trace_id,
            blink::mojom::CacheStorage::OpenCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::string utf8_cache_name = base::UTF16ToUTF8(cache_name);
    TRACE_EVENT_WITH_FLOW1("CacheStorage",
                           "CacheStorageDispatchHost::CacheStorageImpl::Open",
                           TRACE_ID_GLOBAL(trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "cache_name", utf8_cache_name);
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    auto cb = base::BindOnce(
        [](base::WeakPtr<CacheStorageImpl> self, base::TimeTicks start_time,
           int64_t trace_id, blink::mojom::CacheStorage::OpenCallback callback,
           CacheStorageCacheHandle cache_handle, CacheStorageError error) {
          if (!self)
            return;

          UMA_HISTOGRAM_LONG_TIMES(
              "ServiceWorkerCache.CacheStorage.Browser.Open",
              base::TimeTicks::Now() - start_time);

          TRACE_EVENT_WITH_FLOW1(
              "CacheStorage",
              "CacheStorageDispatchHost::CacheStorageImpl::Open::Callback",
              TRACE_ID_GLOBAL(trace_id),
              TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
              CacheStorageTracedValue(error));

          if (error != CacheStorageError::kSuccess) {
            std::move(callback).Run(blink::mojom::OpenResult::NewStatus(error));
            return;
          }

          mojo::PendingAssociatedRemote<blink::mojom::CacheStorageCache>
              pending_remote;
          mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
              coep_reporter;
          if (self->coep_reporter_) {
            self->coep_reporter_->Clone(
                coep_reporter.InitWithNewPipeAndPassReceiver());
          }
          auto cache_impl = std::make_unique<CacheImpl>(
              std::move(cache_handle), self->origin_,
              self->cross_origin_embedder_policy_, std::move(coep_reporter));
          self->owner_->AddCacheReceiver(
              std::move(cache_impl),
              pending_remote.InitWithNewEndpointAndPassReceiver());

          std::move(callback).Run(
              blink::mojom::OpenResult::NewCache(std::move(pending_remote)));
        },
        weak_factory_.GetWeakPtr(), base::TimeTicks::Now(), trace_id,
        std::move(callback));

    if (!cache_storage) {
      std::move(cb).Run(CacheStorageCacheHandle(),
                        MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }

    cache_storage->OpenCache(utf8_cache_name, trace_id, std::move(cb));
  }

 private:
  // Helper method that returns the current CacheStorageHandle value.  If the
  // handle is closed, then it attempts to open a new CacheStorageHandle
  // automatically.  This automatic open is necessary to re-attach to the
  // backend after the browser storage has been wiped.
  content::CacheStorage* GetOrCreateCacheStorage() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(owner_);
    if (!cache_storage_handle_.value())
      cache_storage_handle_ = owner_->OpenCacheStorage(origin_);
    return cache_storage_handle_.value();
  }

  // Owns this.
  CacheStorageDispatcherHost* const owner_;

  const url::Origin origin_;
  const CrossOriginEmbedderPolicy cross_origin_embedder_policy_;
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;
  CacheStorageHandle cache_storage_handle_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CacheStorageImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CacheStorageImpl);
};

CacheStorageDispatcherHost::CacheStorageDispatcherHost() = default;

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_ = context;
}

void CacheStorageDispatcherHost::AddReceiver(
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool incognito = context_ ? context_->is_incognito() : false;
  auto impl = std::make_unique<CacheStorageImpl>(this, origin, incognito,
                                                 cross_origin_embedder_policy,
                                                 std::move(coep_reporter));
  receivers_.Add(std::move(impl), std::move(receiver));
}

void CacheStorageDispatcherHost::AddCacheReceiver(
    std::unique_ptr<CacheImpl> cache_impl,
    mojo::PendingAssociatedReceiver<blink::mojom::CacheStorageCache> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_receivers_.Add(std::move(cache_impl), std::move(receiver));
}

CacheStorageHandle CacheStorageDispatcherHost::OpenCacheStorage(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_ || !OriginCanAccessCacheStorage(origin))
    return CacheStorageHandle();

  scoped_refptr<CacheStorageManager> manager = context_->CacheManager();
  if (!manager)
    return CacheStorageHandle();

  return manager->OpenCacheStorage(origin, CacheStorageOwner::kCacheAPI);
}

}  // namespace content
