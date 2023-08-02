// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"

#include <stddef.h>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebIDBCursor::WebIDBCursor(
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
    int64_t transaction_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : transaction_id_(transaction_id) {
  cursor_.Bind(std::move(cursor_info), std::move(task_runner));
  IndexedDBDispatcher::RegisterCursor(this);
}

WebIDBCursor::~WebIDBCursor() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::UnregisterCursor(this);
}

void WebIDBCursor::Advance(uint32_t count, IDBRequest* request) {
  if (count <= prefetch_keys_.size()) {
    CachedAdvance(count, request);
    return;
  }
  ResetPrefetchCache();

  // Reset all cursor prefetch caches except for this cursor.
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, this);

  cursor_->Advance(
      count, WTF::BindOnce(&WebIDBCursor::AdvanceCallback,
                           WTF::Unretained(this), WrapWeakPersistent(request)));
}

void WebIDBCursor::AdvanceCallback(IDBRequest* request,
                                   mojom::blink::IDBCursorResultPtr result) {
  // May be null in tests.
  if (request) {
    request->OnAdvanceCursor(std::move(result));
  }
}

void WebIDBCursor::CursorContinue(const IDBKey* key,
                                  const IDBKey* primary_key,
                                  IDBRequest* request) {
  DCHECK(key && primary_key);

  if (key->GetType() == mojom::blink::IDBKeyType::None &&
      primary_key->GetType() == mojom::blink::IDBKeyType::None) {
    // No key(s), so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.empty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(request);
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      ++pending_onsuccess_callbacks_;

      cursor_->Prefetch(
          prefetch_amount_,
          WTF::BindOnce(&WebIDBCursor::PrefetchCallback, WTF::Unretained(this),
                        WrapWeakPersistent(request)));

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
  cursor_->CursorContinue(
      IDBKey::Clone(key), IDBKey::Clone(primary_key),
      WTF::BindOnce(&WebIDBCursor::AdvanceCallback, WTF::Unretained(this),
                    WrapWeakPersistent(request)));
}

void WebIDBCursor::PrefetchCallback(IDBRequest* request,
                                    mojom::blink::IDBCursorResultPtr result) {
  if (!result->is_error_result() && !result->is_empty() &&
      result->get_values()->keys.size() ==
          result->get_values()->primary_keys.size() &&
      result->get_values()->keys.size() ==
          result->get_values()->values.size()) {
    SetPrefetchData(std::move(result->get_values()->keys),
                    std::move(result->get_values()->primary_keys),
                    std::move(result->get_values()->values));
    CachedContinue(request);
  } else if (request) {
    // This is the error case. We want error handling to match the AdvanceCursor
    // case.
    request->OnAdvanceCursor(std::move(result));
  }
}

void WebIDBCursor::PostSuccessHandlerCallback() {
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue()/advance() on the cursor
  // again, and that request was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented. If not, it means the
  // callback did something else, or nothing at all, in which case we need to
  // reset the cache.

  if (pending_onsuccess_callbacks_ == 0)
    ResetPrefetchCache();
}

void WebIDBCursor::SetPrefetchData(Vector<std::unique_ptr<IDBKey>> keys,
                                   Vector<std::unique_ptr<IDBKey>> primary_keys,
                                   Vector<std::unique_ptr<IDBValue>> values) {
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

void WebIDBCursor::CachedAdvance(uint32_t count, IDBRequest* request) {
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

  CachedContinue(request);
}

void WebIDBCursor::CachedContinue(IDBRequest* request) {
  DCHECK_GT(prefetch_keys_.size(), 0ul);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  std::unique_ptr<IDBKey> key = std::move(prefetch_keys_.back());
  std::unique_ptr<IDBKey> primary_key =
      std::move(prefetch_primary_keys_.back());
  std::unique_ptr<IDBValue> value = std::move(prefetch_values_.back());

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

  // May be null in tests.
  if (request) {
    // Since the cached request is not round tripping through the browser
    // process, the request has to be explicitly queued. See step 11 of
    // https://www.w3.org/TR/IndexedDB/#dom-idbcursor-continue
    // This is prevented from becoming out-of-order with other requests that
    // do travel through the browser process by the fact that any previous
    // request currently making its way through the browser would have already
    // cleared this cache via `ResetCursorPrefetchCaches()`.
    request->GetExecutionContext()
        ->GetTaskRunner(TaskType::kDatabaseAccess)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&IDBRequest::HandleResponseAdvanceCursor,
                                 WrapWeakPersistent(request), std::move(key),
                                 std::move(primary_key), std::move(value)));
  }
}

void WebIDBCursor::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (prefetch_keys_.empty()) {
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

}  // namespace blink
