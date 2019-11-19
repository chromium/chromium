/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/idb_object_store_or_idb_index.h"
#include "third_party/blink/renderer/bindings/modules/v8/idb_object_store_or_idb_index_or_idb_cursor.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class DOMException;
class ExceptionState;
class IDBCursor;
struct IDBDatabaseMetadata;
class IDBValue;

class MODULES_EXPORT IDBRequest : public EventTargetWithInlineData,
                                  public ActiveScriptWrappable<IDBRequest>,
                                  public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(IDBRequest);

 public:
  using Source = IDBObjectStoreOrIDBIndexOrIDBCursor;
  // Container for async tracing state.
  //
  // The documentation for TRACE_EVENT_ASYNC_{BEGIN,END} suggests identifying
  // trace events by using pointers or a counter that is always incremented on
  // the same thread. This is not viable for IndexedDB, because the same object
  // can result in multiple trace events (requests associated with cursors), and
  // IndexedDB can be used from multiple threads in the same renderer (workers).
  // Furthermore, we want to record the beginning event of an async trace right
  // when we start serving an IDB API call, before the IDBRequest object is
  // created, so we can't rely on information in an IDBRequest.
  //
  // This class solves the ID uniqueness problem by relying on an atomic counter
  // to generating unique IDs in a threadsafe manner. The atomic machinery is
  // used when tracing is enabled. The recording problem is solved by having
  // instances of this class store the information needed to record async trace
  // end events (via TRACE_EVENT_ASYNC_END).
  //
  // From a mechanical perspective, creating an AsyncTraceState instance records
  // the beginning event of an async trace. The instance is then moved into an
  // IDBRequest, which records the async trace's end event at the right time.
  class MODULES_EXPORT AsyncTraceState {
   public:
    // Creates an empty instance, which does not produce any tracing events.
    //
    // This is used for internal requests that should not show up in an
    // application's trace. Examples of internal requests are the requests
    // issued by DevTools, and the requests used to populate indexes.
    explicit AsyncTraceState() = default;

    // Creates an instance that produces begin/end events with the given name.
    //
    // The string pointed to by tracing_name argument must live for the entire
    // application. The easiest way to meet this requirement is to have it be a
    // string literal.
    explicit AsyncTraceState(const char* trace_event_name);
    ~AsyncTraceState();

    // Used to transfer the trace end event state to an IDBRequest.
    AsyncTraceState(AsyncTraceState&& other) {
      DCHECK(IsEmpty());
      this->trace_event_name_ = other.trace_event_name_;
      this->id_ = other.id_;
      other.trace_event_name_ = nullptr;
    }
    AsyncTraceState& operator=(AsyncTraceState&& rhs) {
      DCHECK(IsEmpty());
      this->trace_event_name_ = rhs.trace_event_name_;
      this->id_ = rhs.id_;
      rhs.trace_event_name_ = nullptr;
      return *this;
    }

    // True if this instance does not store information for a tracing end event.
    //
    // An instance is cleared when RecordAndReset() is called on it, or when its
    // state is moved into a different instance. Empty instances are also
    // produced by the AsyncStateTrace() constructor.
    bool IsEmpty() const { return !trace_event_name_; }

    // Records the trace end event whose information is stored in this instance.
    //
    // The method results in the completion of the async trace tracked by this
    // instance, so the instance is cleared.
    void RecordAndReset();

   protected:  // For testing
    const char* trace_event_name() const { return trace_event_name_; }
    size_t id() const { return id_; }

    // Populates the instance with state for a new async trace.
    //
    // The method uses the given even name and generates a new unique ID. The
    // newly generated unique ID is returned.
    size_t PopulateForNewEvent(const char* trace_event_name);

   private:
    // The name of the async trace events tracked by this instance.
    //
    // Null is used to signal that the instance is empty, so the event name
    // cannot be null.
    const char* trace_event_name_ = nullptr;
    // Uniquely generated ID that ties an async trace's begin and end events.
    size_t id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(AsyncTraceState);
  };

  static IDBRequest* Create(ScriptState*,
                            IDBIndex* source,
                            IDBTransaction*,
                            AsyncTraceState);
  static IDBRequest* Create(ScriptState*,
                            IDBObjectStore* source,
                            IDBTransaction*,
                            AsyncTraceState);
  static IDBRequest* Create(ScriptState*,
                            IDBCursor*,
                            IDBTransaction* source,
                            AsyncTraceState);
  static IDBRequest* Create(ScriptState*,
                            const Source&,
                            IDBTransaction*,
                            AsyncTraceState);

  IDBRequest(ScriptState*, const Source&, IDBTransaction*, AsyncTraceState);
  ~IDBRequest() override;

  void Trace(blink::Visitor*) override;

  v8::Isolate* GetIsolate() const { return isolate_; }
  ScriptValue result(ScriptState*, ExceptionState&);
  DOMException* error(ExceptionState&) const;
  void source(ScriptState*, Source&) const;
  IDBTransaction* transaction() const { return transaction_.Get(); }

  bool isResultDirty() const { return result_dirty_; }
  IDBAny* ResultAsAny() const { return result_; }

  // Requests made during index population are implementation details and so
  // events should not be visible to script.
  void PreventPropagation() { prevent_propagation_ = true; }

  // Defined in the IDL
  enum ReadyState { PENDING = 1, DONE = 2, kEarlyDeath = 3 };

  const String& readyState() const;

  // Returns a new WebIDBCallbacks for this request.
  //
  // Each call must be paired with a WebCallbacksDestroyed() call. Most requests
  // have a single WebIDBCallbacks instance created for them.
  //
  // Requests used to open and iterate cursors are special, because they are
  // reused between openCursor() and continue() / advance() calls. These
  // requests have a new WebIDBCallbacks instance created for each of the
  // above-mentioned calls that they are involved in.
  std::unique_ptr<WebIDBCallbacks> CreateWebCallbacks();
  void WebCallbacksDestroyed() {
    DCHECK(web_callbacks_);
    web_callbacks_ = nullptr;
  }
#if DCHECK_IS_ON()
  WebIDBCallbacks* WebCallbacks() const { return web_callbacks_; }
#endif  // DCHECK_IS_ON()

  DEFINE_ATTRIBUTE_EVENT_LISTENER(success, kSuccess)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)

  void SetCursorDetails(indexed_db::CursorType, mojom::IDBCursorDirection);
  void SetPendingCursor(IDBCursor*);
  void Abort();

  // Blink's delivery of results from IndexedDB's backing store to script is
  // more complicated than prescribed in the IndexedDB specification.
  //
  // IDBValue, which holds responses from the backing store, is either the
  // serialized V8 value, or a reference to a Blob that holds the serialized
  // value. IDBValueWrapping.h has the motivation and details. This introduces
  // the following complexities.
  //
  // 1) De-serialization is expensive, so it is done lazily in
  // IDBRequest::result(), which is called synchronously from script. On the
  // other hand, Blob data can only be fetched asynchronously. So, IDBValues
  // that reference serialized data stored in Blobs must be processed before
  // IDBRequest event handlers are invoked, because the event handler script may
  // call IDBRequest::result().
  //
  // 2) The IDBRequest events must be dispatched (enqueued in DOMWindow's event
  // queue) in the order in which the requests were issued. If an IDBValue
  // references a Blob, the Blob processing must block event dispatch for all
  // following IDBRequests in the same transaction.
  //
  // The Blob de-referencing and IDBRequest blocking is performed in the
  // HandleResponse() overloads below. Each HandleResponse() overload is paired
  // with a matching EnqueueResponse() overload, which is called when an
  // IDBRequest's result event can be delivered to the application. All the
  // HandleResponse() variants include a fast path that calls directly into
  // EnqueueResponse() if no queueing is required.
  //
  // Some types of requests, such as indexedDB.openDatabase(), cannot be issued
  // after a request that needs Blob processing, so their results are handled by
  // having WebIDBCallbacksImpl call directly into EnqueueResponse(),
  // EnqueueBlocked(), or EnqueueUpgradeNeeded().

  void HandleResponse(DOMException*);
  void HandleResponse(std::unique_ptr<IDBKey>);
  void HandleResponse(std::unique_ptr<WebIDBCursor>,
                      std::unique_ptr<IDBKey>,
                      std::unique_ptr<IDBKey> primary_key,
                      std::unique_ptr<IDBValue>);
  void HandleResponse(std::unique_ptr<IDBKey>,
                      std::unique_ptr<IDBKey> primary_key,
                      std::unique_ptr<IDBValue>);
  void HandleResponse(std::unique_ptr<IDBValue>);
  void HandleResponse(Vector<std::unique_ptr<IDBValue>>);
  void HandleResponse(int64_t);
  void HandleResponse();

  // Only used in webkitGetDatabaseNames(), which is deprecated and hopefully
  // going away soon.
  void EnqueueResponse(const Vector<String>&);

  // Only IDBOpenDBRequest instances should receive these:
  virtual void EnqueueBlocked(int64_t old_version) { NOTREACHED(); }
  virtual void EnqueueUpgradeNeeded(int64_t old_version,
                                    std::unique_ptr<WebIDBDatabase>,
                                    const IDBDatabaseMetadata&,
                                    mojom::IDBDataLoss,
                                    String data_loss_message) {
    NOTREACHED();
  }
  virtual void EnqueueResponse(std::unique_ptr<WebIDBDatabase>,
                               const IDBDatabaseMetadata&) {
    NOTREACHED();
  }

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const final;

  // Called by a version change transaction that has finished to set this
  // request back from DONE (following "upgradeneeded") back to PENDING (for
  // the upcoming "success" or "error").
  void TransactionDidFinishAndDispatch();

  IDBCursor* GetResultCursor() const;

  // Used to hang onto Blobs until the browser process handles the request.
  //
  // Blobs are ref-counted on the browser side, and BlobDataHandles manage
  // references from renderers. When a BlobDataHandle gets destroyed, the
  // browser-side Blob gets derefenced, which might cause it to be destroyed as
  // well.
  //
  // After script uses a Blob in a put() request, the Blink-side Blob object
  // (which hangs onto the BlobDataHandle) may get garbage-collected. IDBRequest
  // needs to hang onto the BlobDataHandle as well, to avoid having the
  // browser-side Blob get destroyed before the IndexedDB request is processed.
  inline Vector<scoped_refptr<BlobDataHandle>>& transit_blob_handles() {
    return transit_blob_handles_;
  }

#if DCHECK_IS_ON()
  inline bool TransactionHasQueuedResults() const {
    return transaction_ && transaction_->HasQueuedResults();
  }
#endif  // DCHECK_IS_ON()

#if DCHECK_IS_ON()
  inline IDBRequestQueueItem* QueueItem() const { return queue_item_; }
#endif  // DCHECK_IS_ON()

  void AssignNewMetrics(AsyncTraceState metrics) {
    DCHECK(metrics_.IsEmpty());
    metrics_ = std::move(metrics);
  }

 protected:
  void EnqueueEvent(Event*);
  virtual bool ShouldEnqueueEvent() const;
  void EnqueueResultInternal(IDBAny*);
  void SetResult(IDBAny*);

  // Overridden by IDBOpenDBRequest.
  virtual void EnqueueResponse(int64_t);

  // EventTarget
  DispatchEventResult DispatchEventInternal(Event&) override;

  // Can be nullptr for requests that are not associated with a transaction,
  // i.e. delete requests and completed or unsuccessful open requests.
  Member<IDBTransaction> transaction_;

  ReadyState ready_state_ = PENDING;
  bool request_aborted_ = false;  // May be aborted by transaction then receive
                                  // async onsuccess; ignore vs. assert.
  // Maintain the isolate so that all externally allocated memory can be
  // registered against it.
  v8::Isolate* isolate_;

  AsyncTraceState metrics_;

 private:
  // Calls EnqueueResponse().
  friend class IDBRequestQueueItem;

  void SetResultCursor(IDBCursor*,
                       std::unique_ptr<IDBKey>,
                       std::unique_ptr<IDBKey> primary_key,
                       std::unique_ptr<IDBValue>);

  void EnqueueResponse(DOMException*);
  void EnqueueResponse(std::unique_ptr<IDBKey>);
  void EnqueueResponse(std::unique_ptr<WebIDBCursor>,
                       std::unique_ptr<IDBKey>,
                       std::unique_ptr<IDBKey> primary_key,
                       std::unique_ptr<IDBValue>);
  void EnqueueResponse(std::unique_ptr<IDBKey>,
                       std::unique_ptr<IDBKey> primary_key,
                       std::unique_ptr<IDBValue>);
  void EnqueueResponse(std::unique_ptr<IDBValue>);
  void EnqueueResponse(Vector<std::unique_ptr<IDBValue>>);
  void EnqueueResponse();

  void ClearPutOperationBlobs() { transit_blob_handles_.clear(); }

  Source source_;
  Member<IDBAny> result_;
  Member<DOMException> error_;

  bool has_pending_activity_ = true;
  Member<EventQueue> event_queue_;

  // Only used if the result type will be a cursor.
  indexed_db::CursorType cursor_type_ = indexed_db::kCursorKeyAndValue;
  mojom::IDBCursorDirection cursor_direction_ = mojom::IDBCursorDirection::Next;
  // When a cursor is continued/advanced, |result_| is cleared and
  // |pendingCursor_| holds it.
  Member<IDBCursor> pending_cursor_;
  // New state is not applied to the cursor object until the event is
  // dispatched.
  std::unique_ptr<IDBKey> cursor_key_;
  std::unique_ptr<IDBKey> cursor_primary_key_;
  std::unique_ptr<IDBValue> cursor_value_;

  Vector<scoped_refptr<BlobDataHandle>> transit_blob_handles_;

  bool did_fire_upgrade_needed_event_ = false;
  bool prevent_propagation_ = false;
  bool result_dirty_ = true;

  // Pointer back to the WebIDBCallbacks that holds a persistent reference to
  // this object.
  WebIDBCallbacks* web_callbacks_ = nullptr;

  // Non-null while this request is queued behind other requests that are still
  // getting post-processed.
  //
  // The IDBRequestQueueItem is owned by the result queue in IDBTransaction.
  IDBRequestQueueItem* queue_item_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_REQUEST_H_
