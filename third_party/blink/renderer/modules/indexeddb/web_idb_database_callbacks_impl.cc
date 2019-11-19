/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_observation.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

WebIDBDatabaseCallbacksImpl::WebIDBDatabaseCallbacksImpl(
    IDBDatabaseCallbacks* callbacks)
    : callbacks_(callbacks) {}

WebIDBDatabaseCallbacksImpl::~WebIDBDatabaseCallbacksImpl() {
  if (callbacks_)
    callbacks_->WebCallbacksDestroyed();
}

void WebIDBDatabaseCallbacksImpl::OnForcedClose() {
  if (callbacks_)
    callbacks_->OnForcedClose();
}

void WebIDBDatabaseCallbacksImpl::OnVersionChange(int64_t old_version,
                                                  int64_t new_version) {
  if (callbacks_)
    callbacks_->OnVersionChange(old_version, new_version);
}

void WebIDBDatabaseCallbacksImpl::OnAbort(int64_t transaction_id,
                                          const IDBDatabaseError& error) {
  if (callbacks_) {
    callbacks_->OnAbort(
        transaction_id,
        MakeGarbageCollected<DOMException>(
            static_cast<DOMExceptionCode>(error.Code()), error.Message()));
  }
}

void WebIDBDatabaseCallbacksImpl::OnComplete(int64_t transaction_id) {
  if (callbacks_)
    callbacks_->OnComplete(transaction_id);
}

void WebIDBDatabaseCallbacksImpl::OnChanges(
    const ObservationIndexMap& observation_index_map,
    Vector<Persistent<IDBObservation>> observations,
    const TransactionMap& transactions) {
  if (callbacks_) {
    callbacks_->OnChanges(observation_index_map, std::move(observations),
                          transactions);
  }
}

void WebIDBDatabaseCallbacksImpl::Detach() {
  callbacks_.Clear();
}

}  // namespace blink
