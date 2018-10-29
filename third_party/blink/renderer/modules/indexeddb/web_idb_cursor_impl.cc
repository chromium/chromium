// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBKey;
using blink::WebIDBKeyView;
using blink::WebIDBValue;
using blink::mojom::blink::IDBCallbacksAssociatedPtrInfo;
using blink::mojom::blink::IDBCursorAssociatedPtrInfo;

namespace blink {

WebIDBCursorImpl::WebIDBCursorImpl(
    mojom::blink::IDBCursorAssociatedPtrInfo cursor_info,
    int64_t transaction_id)
    : transaction_id_(transaction_id),
      cursor_(std::move(cursor_info)),
      continue_count_(0),
      used_prefetches_(0),
      pending_onsuccess_callbacks_(0),
      prefetch_amount_(kMinPrefetchAmount),
      weak_factory_(this) {
  IndexedDBDispatcher::RegisterCursor(this);
}

WebIDBCursorImpl::~WebIDBCursorImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::UnregisterCursor(this);
}

void WebIDBCursorImpl::Advance(unsigned long count,
                               WebIDBCallbacks* callbacks_ptr) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  if (count <= prefetch_keys_.size()) {
    CachedAdvance(count, callbacks.get());
    return;
  }
  ResetPrefetchCache();

  // Reset all cursor prefetch caches except for this cursor.
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, this);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      std::move(callbacks), transaction_id_, weak_factory_.GetWeakPtr());
  cursor_->Advance(count, GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBCursorImpl::CursorContinue(WebIDBKeyView key,
                                      WebIDBKeyView primary_key,
                                      WebIDBCallbacks* callbacks_ptr) {
  std::unique_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (key.KeyType() == kWebIDBKeyTypeNull &&
      primary_key.KeyType() == kWebIDBKeyTypeNull) {
    // No key(s), so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.IsEmpty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(callbacks.get());
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      ++pending_onsuccess_callbacks_;

      auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
          std::move(callbacks), transaction_id_, weak_factory_.GetWeakPtr());
      cursor_->Prefetch(prefetch_amount_,
                        GetCallbacksProxy(std::move(callbacks_impl)));

      // Increase prefetch_amount_ exponentially.
      prefetch_amount_ *= 2;
      if (prefetch_amount_ > kMaxPrefetchAmount)
        prefetch_amount_ = kMaxPrefetchAmount;

      return;
    }
  } else {
    // Key argument supplied. We couldn't prefetch this.
    ResetPrefetchCache();
  }

  // Reset all cursor prefetch caches except for this cursor.
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, this);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      std::move(callbacks), transaction_id_, weak_factory_.GetWeakPtr());
  cursor_->CursorContinue(WebIDBKeyBuilder::Build(key),
                          WebIDBKeyBuilder::Build(primary_key),
                          GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBCursorImpl::PostSuccessHandlerCallback() {
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue()/advance() on the cursor
  // again, and that request was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented. If not, it means the
  // callback did something else, or nothing at all, in which case we need to
  // reset the cache.

  if (pending_onsuccess_callbacks_ == 0)
    ResetPrefetchCache();
}

void WebIDBCursorImpl::SetPrefetchData(Vector<WebIDBKey> keys,
                                       Vector<WebIDBKey> primary_keys,
                                       Vector<WebIDBValue> values) {
  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  prefetch_keys_.AppendRange(std::make_move_iterator(keys.rbegin()),
                             std::make_move_iterator(keys.rend()));
  prefetch_primary_keys_.AppendRange(
      std::make_move_iterator(primary_keys.rbegin()),
      std::make_move_iterator(primary_keys.rend()));
  prefetch_values_.AppendRange(std::make_move_iterator(values.rbegin()),
                               std::make_move_iterator(values.rend()));

  used_prefetches_ = 0;
  pending_onsuccess_callbacks_ = 0;
}

void WebIDBCursorImpl::CachedAdvance(unsigned long count,
                                     WebIDBCallbacks* callbacks) {
  DCHECK_GE(prefetch_keys_.size(), count);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  while (count > 1) {
    prefetch_keys_.pop_back();
    prefetch_primary_keys_.pop_back();
    prefetch_values_.pop_back();
    ++used_prefetches_;
    --count;
  }

  CachedContinue(callbacks);
}

void WebIDBCursorImpl::CachedContinue(WebIDBCallbacks* callbacks) {
  DCHECK_GT(prefetch_keys_.size(), 0ul);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  WebIDBKey key = std::move(prefetch_keys_.back());
  WebIDBKey primary_key = std::move(prefetch_primary_keys_.back());
  WebIDBValue value = std::move(prefetch_values_.back());

  prefetch_keys_.pop_back();
  prefetch_primary_keys_.pop_back();
  prefetch_values_.pop_back();
  ++used_prefetches_;

  ++pending_onsuccess_callbacks_;

  if (!continue_count_) {
    // The cache was invalidated by a call to ResetPrefetchCache()
    // after the RequestIDBCursorPrefetch() was made. Now that the
    // initiating continue() call has been satisfied, discard
    // the rest of the cache.
    ResetPrefetchCache();
  }

  callbacks->OnSuccess(std::move(key), std::move(primary_key),
                       std::move(value));
}

void WebIDBCursorImpl::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (prefetch_keys_.IsEmpty()) {
    // No prefetch cache, so no need to reset the cursor in the back-end.
    return;
  }

  // Reset the back-end cursor.
  cursor_->PrefetchReset(used_prefetches_, prefetch_keys_.size());

  // Reset the prefetch cache.
  prefetch_keys_.clear();
  prefetch_primary_keys_.clear();
  prefetch_values_.clear();

  pending_onsuccess_callbacks_ = 0;
}

IDBCallbacksAssociatedPtrInfo WebIDBCursorImpl::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace blink
