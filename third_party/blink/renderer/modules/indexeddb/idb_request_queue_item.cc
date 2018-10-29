// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_loader.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"

namespace blink {

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
    loader_ = std::make_unique<IDBRequestLoader>(this, values_);
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
    loader_ = std::make_unique<IDBRequestLoader>(this, values_);
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
    loader_ = std::make_unique<IDBRequestLoader>(this, values_);
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
    loader_ = std::make_unique<IDBRequestLoader>(this, values_);
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
  if (request_->request_aborted_) {
    // The backing store can get the result back to the request after it's been
    // aborted due to a transaction abort. In this case, we can't rely on
    // IDBRequest::Abort() to call CancelLoading().

    // Setting loader_ to null here makes sure we don't call Cancel() on a
    // IDBRequestLoader that hasn't been Start()ed. The current implementation
    // behaves well even if Cancel() is called without Start() being called, but
    // this reset makes the IDBRequestLoader lifecycle easier to reason about.
    loader_.reset();

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

  if (loader_) {
    loader_->Cancel();
    loader_.reset();

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

    case kVoid:
      DCHECK_EQ(values_.size(), 0U);
      request_->EnqueueResponse();
      break;
  }
}

}  // namespace blink
