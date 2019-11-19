// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/cursor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

namespace content {

CursorImpl::CursorImpl(std::unique_ptr<IndexedDBCursor> cursor,
                       const url::Origin& origin,
                       IndexedDBDispatcherHost* dispatcher_host,
                       scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      origin_(origin),
      idb_runner_(std::move(idb_runner)),
      cursor_(std::move(cursor)) {
  DCHECK(idb_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CursorImpl::~CursorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CursorImpl::Advance(uint32_t count,
                         blink::mojom::IDBCursor::AdvanceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->Advance(count, dispatcher_host_->AsWeakPtr(), std::move(callback));
}

void CursorImpl::CursorContinue(
    const blink::IndexedDBKey& key,
    const blink::IndexedDBKey& primary_key,
    blink::mojom::IDBCursor::CursorContinueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->Continue(
      dispatcher_host_->AsWeakPtr(),
      key.IsValid() ? std::make_unique<blink::IndexedDBKey>(key) : nullptr,
      primary_key.IsValid() ? std::make_unique<blink::IndexedDBKey>(primary_key)
                            : nullptr,
      std::move(callback));
}

void CursorImpl::Prefetch(int32_t count,
                          blink::mojom::IDBCursor::PrefetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->PrefetchContinue(dispatcher_host_->AsWeakPtr(), count,
                            std::move(callback));
}

void CursorImpl::PrefetchReset(int32_t used_prefetches,
                               int32_t unused_prefetches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status s =
      cursor_->PrefetchReset(used_prefetches, unused_prefetches);
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!s.ok())
    DLOG(ERROR) << "Unable to reset prefetch";
}

void CursorImpl::OnRemoveBinding(base::OnceClosure remove_binding_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->OnRemoveBinding(std::move(remove_binding_cb));
}

}  // namespace content
