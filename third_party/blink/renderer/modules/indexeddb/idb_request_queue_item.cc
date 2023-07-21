// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_loader.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class IDBDatabaseGetAllResultSinkImpl
    : public mojom::blink::IDBDatabaseGetAllResultSink {
 public:
  IDBDatabaseGetAllResultSinkImpl(
      mojo::PendingReceiver<mojom::blink::IDBDatabaseGetAllResultSink> receiver,
      IDBRequestQueueItem* owner,
      bool key_only)
      : receiver_(this, std::move(receiver)),
        owner_(owner),
        key_only_(key_only) {
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &IDBDatabaseGetAllResultSinkImpl::OnDisconnect, WTF::Unretained(this)));
  }

  ~IDBDatabaseGetAllResultSinkImpl() override = default;

  bool IsWaiting() const { return receiver_.is_bound(); }

  void OnDisconnect() {
    // Force IsWaiting to be false.
    receiver_.reset();

    if (key_only_) {
      owner_->response_type_ = IDBRequestQueueItem::kKey;
      owner_->key_ = IDBKey::CreateArray(std::move(keys_));
      owner_->OnResultLoadComplete();
    } else {
      owner_->response_type_ = IDBRequestQueueItem::kValueArray;

      Vector<std::unique_ptr<IDBValue>> idb_values;
      idb_values.ReserveInitialCapacity(values_.size());
      for (const mojom::blink::IDBReturnValuePtr& value : values_) {
        std::unique_ptr<IDBValue> idb_value =
            IDBValue::ConvertReturnValue(value);
        idb_value->SetIsolate(owner_->request_->GetIsolate());
        idb_values.emplace_back(std::move(idb_value));
      }

      bool is_wrapped = IDBValueUnwrapper::IsWrapped(idb_values);
      owner_->values_ = std::move(idb_values);
      if (is_wrapped) {
        owner_->loader_ =
            MakeGarbageCollected<IDBRequestLoader>(owner_, owner_->values_);
        if (owner_->started_loading_) {
          // Try again now that the values exist.
          owner_->StartLoading();
        }
      } else {
        owner_->OnResultLoadComplete();
      }
    }
  }

  void ReceiveValues(
      WTF::Vector<mojom::blink::IDBReturnValuePtr> values) override {
    DCHECK(!key_only_);
    DCHECK_LE(values.size(),
              static_cast<wtf_size_t>(mojom::blink::kIDBGetAllChunkSize));
    if (values_.empty()) {
      values_ = std::move(values);
      return;
    }

    values_.reserve(values_.size() + values.size());
    for (auto& value : values)
      values_.emplace_back(std::move(value));
  }

  void ReceiveKeys(WTF::Vector<std::unique_ptr<IDBKey>> keys) override {
    DCHECK(key_only_);
    DCHECK_LE(keys.size(),
              static_cast<wtf_size_t>(mojom::blink::kIDBGetAllChunkSize));
    if (keys_.empty()) {
      keys_ = std::move(keys);
      return;
    }

    keys_.reserve(keys_.size() + keys.size());
    for (auto& key : keys)
      keys_.emplace_back(std::move(key));
  }

  void OnError(mojom::blink::IDBErrorPtr error) override {
    owner_->response_type_ = IDBRequestQueueItem::kError;
    owner_->error_ = MakeGarbageCollected<DOMException>(
        static_cast<DOMExceptionCode>(error->error_code), error->error_message);
    // Prevent OnDisconnect from sending keys or values.
    receiver_.reset();
    owner_->OnResultLoadComplete();
  }

 private:
  mojo::Receiver<mojom::blink::IDBDatabaseGetAllResultSink> receiver_;
  IDBRequestQueueItem* owner_;
  bool key_only_;

  WTF::Vector<mojom::blink::IDBReturnValuePtr> values_;
  WTF::Vector<std::unique_ptr<IDBKey>> keys_;
};

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    DOMException* error,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      error_(error),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kError),
      ready_(true) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    int64_t value,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      on_result_load_complete_(std::move(on_result_load_complete)),
      int64_value_(value),
      response_type_(kNumber),
      ready_(true) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kVoid),
      ready_(true) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    std::unique_ptr<IDBKey> key,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      key_(std::move(key)),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kKey),
      ready_(true) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    std::unique_ptr<IDBValue> value,
    bool attach_loader,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kValue),
      ready_(!attach_loader) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  values_.push_back(std::move(value));
  if (attach_loader)
    loader_ = MakeGarbageCollected<IDBRequestLoader>(this, values_);
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    Vector<std::unique_ptr<IDBValue>> values,
    bool attach_loader,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      values_(std::move(values)),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kValueArray),
      ready_(!attach_loader) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  if (attach_loader)
    loader_ = MakeGarbageCollected<IDBRequestLoader>(this, values_);
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    Vector<Vector<std::unique_ptr<IDBValue>>> all_values,
    bool attach_loader,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kValueArrayArray),
      ready_(!attach_loader) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;

  all_values_size_info_.ReserveInitialCapacity(all_values.size());
  for (Vector<std::unique_ptr<IDBValue>>& values : all_values) {
    all_values_size_info_.push_back(values.size());
    values_.AppendRange(std::make_move_iterator(values.begin()),
                        std::make_move_iterator(values.end()));
  }

  if (attach_loader) {
    loader_ = MakeGarbageCollected<IDBRequestLoader>(this, values_);
  }
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    std::unique_ptr<IDBValue> value,
    bool attach_loader,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      key_(std::move(key)),
      primary_key_(std::move(primary_key)),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kKeyPrimaryKeyValue),
      ready_(!attach_loader) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  values_.push_back(std::move(value));
  if (attach_loader)
    loader_ = MakeGarbageCollected<IDBRequestLoader>(this, values_);
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    std::unique_ptr<WebIDBCursor> cursor,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    std::unique_ptr<IDBValue> value,
    bool attach_loader,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      key_(std::move(key)),
      primary_key_(std::move(primary_key)),
      cursor_(std::move(cursor)),
      on_result_load_complete_(std::move(on_result_load_complete)),
      response_type_(kCursorKeyPrimaryKeyValue),
      ready_(!attach_loader) {
  DCHECK(on_result_load_complete_);
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  values_.push_back(std::move(value));
  if (attach_loader)
    loader_ = MakeGarbageCollected<IDBRequestLoader>(this, values_);
}

IDBRequestQueueItem::IDBRequestQueueItem(
    IDBRequest* request,
    bool key_only,
    mojo::PendingReceiver<mojom::blink::IDBDatabaseGetAllResultSink> receiver,
    base::OnceClosure on_result_load_complete)
    : request_(request),
      on_result_load_complete_(std::move(on_result_load_complete)),
      ready_(false) {
  DCHECK_EQ(request->queue_item_, nullptr);
  request_->queue_item_ = this;
  get_all_sink_ = std::make_unique<IDBDatabaseGetAllResultSinkImpl>(
      std::move(receiver), this, key_only);
}

IDBRequestQueueItem::~IDBRequestQueueItem() {
#if DCHECK_IS_ON()
  DCHECK(ready_);
  DCHECK(callback_fired_);
#endif  // DCHECK_IS_ON()
}

void IDBRequestQueueItem::OnResultLoadComplete() {
  DCHECK(!ready_);
  ready_ = true;

  DCHECK(on_result_load_complete_);
  std::move(on_result_load_complete_).Run();
}

void IDBRequestQueueItem::OnResultLoadComplete(DOMException* error) {
  DCHECK(!ready_);
  DCHECK(response_type_ != kError);

  response_type_ = kError;
  error_ = error;

  // This is not necessary, but releases non-trivial amounts of memory early.
  values_.clear();

  OnResultLoadComplete();
}

void IDBRequestQueueItem::StartLoading() {
  started_loading_ = true;

  // If waiting on results from get all before loading, early out.
  if (get_all_sink_ && get_all_sink_->IsWaiting())
    return;

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
  }
}

void IDBRequestQueueItem::CancelLoading() {
  if (ready_)
    return;

  if (get_all_sink_)
    get_all_sink_.reset();

  if (loader_) {
    loader_->Cancel();
    loader_.Clear();

    // IDBRequestLoader::Cancel() should not call any of the EnqueueResponse
    // variants.
    DCHECK(!ready_);
  }

  // Mark this item as ready so the transaction's result queue can be drained.
  response_type_ = kCanceled;
  values_.clear();
  OnResultLoadComplete();
}

void IDBRequestQueueItem::EnqueueResponse() {
  DCHECK(ready_);
#if DCHECK_IS_ON()
  DCHECK(!callback_fired_);
  callback_fired_ = true;
#endif  // DCHECK_IS_ON()
  DCHECK_EQ(request_->queue_item_, this);
  request_->queue_item_ = nullptr;

  switch (response_type_) {
    case kCanceled:
      DCHECK_EQ(values_.size(), 0U);
      break;

    case kCursorKeyPrimaryKeyValue:
      DCHECK_EQ(values_.size(), 1U);
      request_->EnqueueResponse(std::move(cursor_), std::move(key_),
                                std::move(primary_key_),
                                std::move(values_.front()));
      break;

    case kError:
      DCHECK(error_);
      request_->EnqueueResponse(error_);
      break;

    case kKeyPrimaryKeyValue:
      DCHECK_EQ(values_.size(), 1U);
      request_->EnqueueResponse(std::move(key_), std::move(primary_key_),
                                std::move(values_.front()));
      break;

    case kKey:
      DCHECK_EQ(values_.size(), 0U);
      request_->EnqueueResponse(std::move(key_));
      break;

    case kNumber:
      DCHECK_EQ(values_.size(), 0U);
      request_->EnqueueResponse(int64_value_);
      break;

    case kValue:
      DCHECK_EQ(values_.size(), 1U);
      request_->EnqueueResponse(std::move(values_.front()));
      break;

    case kValueArray:
      request_->EnqueueResponse(std::move(values_));
      break;

    case kValueArrayArray: {
      // rebuild all_values (2d vector)
      wtf_size_t current_value_idx = 0;
      Vector<Vector<std::unique_ptr<IDBValue>>> all_values;
      for (auto s : all_values_size_info_) {
        Vector<std::unique_ptr<IDBValue>> all_value;
        all_value.AppendRange(
            std::make_move_iterator(values_.begin() + current_value_idx),
            std::make_move_iterator(values_.begin() + current_value_idx + s));
        all_values.push_back(std::move(all_value));
        current_value_idx += s;
      }

      request_->EnqueueResponse(std::move(all_values));
      break;
    }

    case kVoid:
      DCHECK_EQ(values_.size(), 0U);
      request_->EnqueueResponse();
      break;
  }
}

}  // namespace blink
