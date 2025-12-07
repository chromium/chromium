// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_loader.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class IDBDatabaseGetAllResultSinkImpl
    : public mojom::blink::IDBDatabaseGetAllResultSink {
 public:
  IDBDatabaseGetAllResultSinkImpl(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseGetAllResultSink>
          receiver,
      IDBRequestQueueItem* owner,
      mojom::blink::IDBGetAllResultType get_all_result_type)
      : receiver_(this, std::move(receiver)),
        owner_(owner),
        get_all_result_type_(get_all_result_type) {}

  ~IDBDatabaseGetAllResultSinkImpl() override = default;

  bool IsWaiting() const { return active_; }

  void ReceiveResults(Vector<mojom::blink::IDBRecordPtr> results,
                      bool done) override {
    CHECK(active_);
    CHECK_LE(results.size(),
             static_cast<wtf_size_t>(mojom::blink::kIDBGetAllChunkSize));

    // Increase `Vector` capacities to make room for new `results`.
    records_.primary_keys.reserve(records_.primary_keys.size() +
                                  results.size());
    records_.values.reserve(records_.values.size() + results.size());
    records_.index_keys.reserve(records_.index_keys.size() + results.size());

    // Add each record's primary key, value, and index key to `records_`.
    // Depending on `get_all_result_type_`, some of these may remain empty.
    for (auto& result : results) {
      if (result->primary_key) {
        records_.primary_keys.emplace_back(std::move(*result->primary_key));
      }

      if (result->return_value) {
        std::unique_ptr<IDBValue> result_value =
            IDBValue::ConvertReturnValue(result->return_value);
        result_value->SetIsolate(owner_->request_->GetIsolate());
        records_.values.emplace_back(std::move(result_value));
      }

      if (result->index_key) {
        records_.index_keys.emplace_back(std::move(*result->index_key));
      }
    }

    if (!done) {
      return;
    }

    active_ = false;
    owner_->response_type_ = GetResponseType();
    owner_->records_ = std::move(records_);

    if (owner_->MaybeCreateLoader()) {
      if (owner_->started_loading_) {
        // Try again now that the values exist.
        owner_->StartLoading();
      }
    } else {
      owner_->OnResultReady();
    }
  }

  void OnError(mojom::blink::IDBErrorPtr error) override {
    DCHECK(active_);
    DOMException* dom_exception;
    if (error->error_code == mojom::blink::IDBException::kQuotaError &&
        RuntimeEnabledFeatures::QuotaExceededErrorUpdateEnabled()) {
      dom_exception =
          MakeGarbageCollected<QuotaExceededError>(error->error_message);
    } else {
      dom_exception = MakeGarbageCollected<DOMException>(
          static_cast<DOMExceptionCode>(error->error_code),
          error->error_message);
    }
    owner_->response_type_ = IDBRequestQueueItem::kError;
    owner_->error_ = dom_exception;
    active_ = false;
    owner_->OnResultReady();
  }

 private:
  IDBRequestQueueItem::ResponseType GetResponseType() const {
    switch (get_all_result_type_) {
      case mojom::blink::IDBGetAllResultType::Keys:
        return IDBRequestQueueItem::kKeyArray;

      case mojom::blink::IDBGetAllResultType::Values:
        return IDBRequestQueueItem::kValueArray;

      case mojom::blink::IDBGetAllResultType::Records:
        return IDBRequestQueueItem::kRecordArray;
    }
    NOTREACHED();
  }

  mojo::AssociatedReceiver<mojom::blink::IDBDatabaseGetAllResultSink> receiver_;
  raw_ptr<IDBRequestQueueItem> owner_;
  mojom::blink::IDBGetAllResultType get_all_result_type_;

  // Accumulates values in batches for `getAllKeys()`, `getAll()`, and
  // `getAllRecords()` until the remote finishes sending the results.
  // `get_all_result_type_` determines what `records_` contains. For example,
  // when `get_all_result_type_` is `Keys`, `records_` will contain
  // `primary_keys` only with `values` and `index_keys` remaining empty.
  IDBRecordArray records_;

  // True while results are still being received.
  bool active_ = true;
};

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         DOMException* error,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      error_(error),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kError) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         int64_t value,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      on_result_ready_(std::move(on_result_ready)),
      int64_value_(value),
      response_type_(kNumber) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kVoid) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         std::unique_ptr<IDBKey> key,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      key_(std::move(key)),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kKey) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         std::unique_ptr<IDBValue> value,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kValue) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  records_.values.push_back(std::move(value));
  MaybeCreateLoader();
}

IDBRequestQueueItem::IDBRequestQueueItem(IDBRequest* request,
                                         std::unique_ptr<IDBKey> key,
                                         std::unique_ptr<IDBKey> primary_key,
                                         std::unique_ptr<IDBValue> value,
                                         base::OnceClosure on_result_ready)
    : request_(request),
      key_(std::move(key)),
      primary_key_(std::move(primary_key)),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kKeyPrimaryKeyValue) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  records_.values.push_back(std::move(value));
  MaybeCreateLoader();
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> pending_cursor,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    std::unique_ptr<IDBValue> value,
    base::OnceClosure on_result_ready)
    : request_(request),
      key_(std::move(key)),
      primary_key_(std::move(primary_key)),
      pending_cursor_(std::move(pending_cursor)),
      on_result_ready_(std::move(on_result_ready)),
      response_type_(kCursorKeyPrimaryKeyValue) {
  DCHECK(on_result_ready_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  records_.values.push_back(std::move(value));
  MaybeCreateLoader();
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    mojom::blink::IDBGetAllResultType get_all_result_type,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseGetAllResultSink>
        receiver,
    base::OnceClosure on_result_ready)
    : request_(request), on_result_ready_(std::move(on_result_ready)) {
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  get_all_sink_ = std::make_unique<IDBDatabaseGetAllResultSinkImpl>(
      std::move(receiver), this, get_all_result_type);
}

IDBRequestQueueItem::~IDBRequestQueueItem() {
#if DCHECK_IS_ON()
  DCHECK(ready_);
  DCHECK(result_sent_);
#endif  // DCHECK_IS_ON()
}

void IDBRequestQueueItem::OnResultReady() {
  CHECK(!ready_);
  ready_ = true;

  CHECK(on_result_ready_);
  std::move(on_result_ready_).Run();
}

bool IDBRequestQueueItem::MaybeCreateLoader() {
  if (IDBValueUnwrapper::IsWrapped(records_.values)) {
    loader_ = MakeGarbageCollected<IDBRequestLoader>(
        std::move(records_.values), request_->GetExecutionContext(),
        blink::BindOnce(&IDBRequestQueueItem::OnLoadComplete,
                        weak_factory_.GetWeakPtr()));
    return true;
  }
  return false;
}

void IDBRequestQueueItem::OnLoadComplete(
    Vector<std::unique_ptr<IDBValue>>&& values,
    DOMException* error) {
  records_.values = std::move(values);

  if (error) {
    DCHECK(!ready_);
    DCHECK(response_type_ != kError);

    response_type_ = kError;
    error_ = error;

    // This is not necessary, but releases non-trivial amounts of memory early.
    records_.clear();
  }

  OnResultReady();
}

void IDBRequestQueueItem::StartLoading() {
  started_loading_ = true;

  // If waiting on results from get all before loading, early out.
  if (get_all_sink_ && get_all_sink_->IsWaiting()) {
    return;
  }

  if (request_->request_aborted_) {
    // The backing store can get the result back to the request after it's been
    // aborted due to a transaction abort. In this case, we can't rely on
    // IDBRequest::Abort() to call CancelLoading().

    // Setting loader_ to null here makes sure we don't call Cancel() on a
    // IDBRequestLoader that hasn't been Start()ed. The current implementation
    // behaves well even if Cancel() is called without Start() being called, but
    // this reset makes the IDBRequestLoader lifecycle easier to reason about.
    loader_.Clear();

    CancelLoading();
    return;
  }

  if (loader_) {
    DCHECK(!ready_);
    loader_->Start();
  } else {
    OnResultReady();
  }
}

void IDBRequestQueueItem::CancelLoading() {
  if (ready_) {
    return;
  }

  if (get_all_sink_) {
    get_all_sink_.reset();
  }

  if (loader_) {
    // Take `loading_values` from `loader_` to destroy the values before garbage
    // collection destroys `loader_`.
    Vector<std::unique_ptr<IDBValue>> loading_values = loader_->Cancel();
    loader_.Clear();

    // IDBRequestLoader::Cancel() should not call any of the SendResult
    // variants.
    DCHECK(!ready_);
  }

  // Mark this item as ready so the transaction's result queue can be drained.
  response_type_ = kCanceled;
  records_.clear();

  OnResultReady();
}

void IDBRequestQueueItem::SendResult() {
  DCHECK(ready_);
#if DCHECK_IS_ON()
  DCHECK(!result_sent_);
  result_sent_ = true;
#endif  // DCHECK_IS_ON()
  CHECK_EQ(request_->queue_item_, this);
  request_->queue_item_ = nullptr;

  switch (response_type_) {
    case kCanceled: {
      break;
    }
    case kCursorKeyPrimaryKeyValue: {
      CHECK_EQ(records_.values.size(), 1U);
      request_->SendResultCursor(std::move(pending_cursor_), std::move(key_),
                                 std::move(primary_key_),
                                 std::move(records_.values.front()));
      break;
    }
    case kError: {
      DCHECK(error_);
      request_->SendError(error_);
      break;
    }
    case kKeyPrimaryKeyValue: {
      CHECK_EQ(records_.values.size(), 1U);
      request_->SendResultAdvanceCursor(std::move(key_),
                                        std::move(primary_key_),
                                        std::move(records_.values.front()));
      break;
    }
    case kKey: {
      if (key_ && key_->IsValid()) {
        request_->SendResult(MakeGarbageCollected<IDBAny>(std::move(key_)));
      } else {
        request_->SendResult(
            MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
      }

      break;
    }
    case kKeyArray: {
      CHECK(records_.values.empty());
      CHECK(records_.index_keys.empty());

      std::unique_ptr<IDBKey> key_array =
          IDBKey::CreateArray(std::move(records_.primary_keys));
      request_->SendResult(MakeGarbageCollected<IDBAny>(std::move(key_array)));
      break;
    }
    case kNumber: {
      request_->SendResult(MakeGarbageCollected<IDBAny>(int64_value_));
      break;
    }
    case kRecordArray: {
      // Each result for `getAllRecords()` must provide a primary key, value and
      // maybe an index key.  `IDBIndex::getAllRecords()` must provide an index
      // key, but `index_keys` must remain empty
      // for`IDBObjectStore::getAllRecords()`.
      CHECK_EQ(records_.primary_keys.size(), records_.values.size());
      CHECK(records_.index_keys.empty() ||
            records_.index_keys.size() == records_.primary_keys.size());

      request_->SendResult(MakeGarbageCollected<IDBAny>(std::move(records_)));
      break;
    }
    case kValue: {
      CHECK_EQ(records_.values.size(), 1U);
      request_->SendResultValue(std::move(records_.values.front()));
      break;
    }
    case kValueArray: {
      CHECK(records_.primary_keys.empty());
      CHECK(records_.index_keys.empty());

      request_->SendResult(
          MakeGarbageCollected<IDBAny>(std::move(records_.values)));
      break;
    }
    case kVoid: {
      request_->SendResult(
          MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
      break;
    }
  }
}

}  // namespace blink
