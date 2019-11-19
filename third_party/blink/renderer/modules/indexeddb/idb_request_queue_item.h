// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_QUEUE_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_QUEUE_ITEM_H_

#include <memory>

#include "base/callback.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMException;
class IDBKey;
class IDBRequest;
class IDBRequestLoader;
class IDBValue;
class WebIDBCursor;

// Queues up a transaction's IDBRequest results for orderly delivery.
//
// The IndexedDB specification requires that the events corresponding to IDB
// request results fire in the order in which the requests were issued. The
// browser-side backend processes requests in order, but the Blink side may need
// to perform post-processing on the results (e.g. large value unwrapping).
// When a result needs post-processing, this queue captures all results received
// during the post-processing steps. The events for these results may only fire
// after the post-processed result's event is fired.
//
// A queue item holds a Persistent (not garbage-collected) reference to the
// IDBRequest whose result it will deliver. This creates a reference cycle,
// because IDBRequest holds a pointer to its IDBTransaction, and IDBTransaction
// stores all pending IDBRequestQueueItem instances for its requests in a queue
// via non-garbage-collected pointers (std::unique_ptr). To avoid leaks, the
// request-processing code must ensure that IDBTransaction's queue gets drained
// at some point, even if the transaction's ExecutionContext goes away. The
// lifecycle tests in IDBTransactionTest aim to cover this requirement.
//
// Given that the cycle above exists, the closures passed to IDBRequestQueueItem
// can safely store Persistent pointers the IDBRequest or to the IDBTransaction.
class IDBRequestQueueItem {
  USING_FAST_MALLOC(IDBRequestQueueItem);

 public:
  IDBRequestQueueItem(IDBRequest*,
                      DOMException*,
                      base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      int64_t,
                      base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*, base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      std::unique_ptr<IDBKey>,
                      base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      std::unique_ptr<IDBValue>,
                      bool attach_loader,
                      base::OnceClosure on_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      Vector<std::unique_ptr<IDBValue>>,
                      bool attach_loader,
                      base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      std::unique_ptr<IDBKey>,
                      std::unique_ptr<IDBKey> primary_key,
                      std::unique_ptr<IDBValue>,
                      bool attach_loader,
                      base::OnceClosure on_result_load_complete);
  IDBRequestQueueItem(IDBRequest*,
                      std::unique_ptr<WebIDBCursor>,
                      std::unique_ptr<IDBKey>,
                      std::unique_ptr<IDBKey> primary_key,
                      std::unique_ptr<IDBValue>,
                      bool attach_loader,
                      base::OnceClosure on_result_load_complete);
  ~IDBRequestQueueItem();

  // False if this result still requires post-processing.
  inline bool IsReady() { return ready_; }

  // The request whose queued result is tracked by this item.
  inline IDBRequest* Request() { return request_; }

  // Starts post-processing the IDBRequest's result.
  //
  // This method must be called after the IDBRequestQueueItem is enqueued into
  // the appropriate queue, because it is possible for the loading operation to
  // complete synchronously, in which case IDBTransaction::OnResultReady() will
  // be called with the (presumably) enqueued IDBRequest before this method
  // returns.
  void StartLoading();

  // Stops post-processing the IDBRequest's result.
  //
  // This method may be called without an associated StartLoading().
  void CancelLoading();

  // Calls the correct EnqueueResponse overload on the associated request.
  //
  // This should only be called by the request's IDBTransaction.
  void EnqueueResponse();

  // Called by the associated IDBRequestLoader when result processing is done.
  void OnResultLoadComplete();

  // Called by the associated IDBRequestLoader when result processing fails.
  void OnResultLoadComplete(DOMException* error);

 private:
  // The IDBRequest callback that will be called for this result.
  enum ResponseType {
    kCanceled,
    kCursorKeyPrimaryKeyValue,
    kError,
    kNumber,
    kKey,
    kKeyPrimaryKeyValue,
    kValue,
    kValueArray,
    kVoid,
  };

  // The IDBRequest that will receive a callback for this result.
  Persistent<IDBRequest> request_;

  // The error argument to the IDBRequest callback.
  //
  // Only used if the mode_ is kError.
  Persistent<DOMException> error_;

  // The key argument to the IDBRequest callback.
  //
  // Only used if mode_ is kKeyPrimaryKeyValue.
  std::unique_ptr<IDBKey> key_;

  // The primary_key argument to the IDBRequest callback.
  //
  // Only used if mode_ is kKeyPrimaryKeyValue.
  std::unique_ptr<IDBKey> primary_key_;

  // All the values that will be passed back to the IDBRequest.
  Vector<std::unique_ptr<IDBValue>> values_;

  // The cursor argument to the IDBRequest callback.
  std::unique_ptr<WebIDBCursor> cursor_;

  // Performs post-processing on this result.
  //
  // nullptr for results that do not require post-processing and for results
  // whose post-processing has completed.
  std::unique_ptr<IDBRequestLoader> loader_;

  // Called when result post-processing has completed.
  base::OnceClosure on_result_load_complete_;

  // The integer value argument to the IDBRequest callback.
  int64_t int64_value_;

  // Identifies the IDBRequest::EnqueueResponse() overload that will be called.
  ResponseType response_type_;

  // False if this result still requires post-processing.
  bool ready_;

#if DCHECK_IS_ON()
  // True if the appropriate EnqueueResponse() method was called in IDBRequest.
  //
  // If CancelLoading() is called, the ResponseType might be kCanceled. In this
  // case, callback_fired_ can be set to true even though no EnqueueResponse()
  // call occurs.
  bool callback_fired_ = false;
#endif  // DCHECK_IS_ON()
};

using IDBRequestQueue = Deque<std::unique_ptr<IDBRequestQueueItem>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_QUEUE_ITEM_H_
