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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_DATABASE_CALLBACKS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_DATABASE_CALLBACKS_H_

#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace blink {

struct WebIDBObservation;

class WebIDBDatabaseCallbacks {
 public:
  using ObservationIndexMap = std::unordered_map<int32_t, WebVector<int32_t>>;

  // Maps observer to transaction, which needs an id and a scope.
  using TransactionMap =
      std::unordered_map<int32_t, std::pair<int64_t, WebVector<int64_t>>>;

  virtual ~WebIDBDatabaseCallbacks() = default;

  virtual void OnForcedClose() = 0;
  virtual void OnVersionChange(long long old_version,
                               long long new_version) = 0;

  virtual void OnAbort(long long transaction_id,
                       const WebIDBDatabaseError&) = 0;
  virtual void OnComplete(long long transaction_id) = 0;
  virtual void OnChanges(const ObservationIndexMap&,
                         WebVector<WebIDBObservation>,
                         const TransactionMap&) = 0;
  virtual void Detach() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_DATABASE_CALLBACKS_H_
