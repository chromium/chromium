/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_

#include <memory>

#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class IDBRequest;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBKey;
struct WebIDBMetadata;
struct WebIDBNameAndVersion;
class WebIDBValue;

class WebIDBCallbacksImpl final : public WebIDBCallbacks {
  USING_FAST_MALLOC(WebIDBCallbacksImpl);

 public:
  static std::unique_ptr<WebIDBCallbacksImpl> Create(IDBRequest*);

  ~WebIDBCallbacksImpl() override;

  // Pointers transfer ownership.
  void OnError(const WebIDBDatabaseError&) override;
  void OnSuccess(const WebVector<WebIDBNameAndVersion>&) override;
  void OnSuccess(const WebVector<WebString>&) override;
  void OnSuccess(WebIDBCursor*,
                 WebIDBKey,
                 WebIDBKey primary_key,
                 WebIDBValue) override;
  void OnSuccess(WebIDBDatabase*, const WebIDBMetadata&) override;
  void OnSuccess(WebIDBKey) override;
  void OnSuccess(WebIDBValue) override;
  void OnSuccess(WebVector<WebIDBValue>) override;
  void OnSuccess(long long) override;
  void OnSuccess() override;
  void OnSuccess(WebIDBKey, WebIDBKey primary_key, WebIDBValue) override;
  void OnBlocked(long long old_version) override;
  void OnUpgradeNeeded(long long old_version,
                       WebIDBDatabase*,
                       const WebIDBMetadata&,
                       unsigned short data_loss,
                       WebString data_loss_message) override;
  void Detach() override;

 private:
  explicit WebIDBCallbacksImpl(IDBRequest*);

  Persistent<IDBRequest> request_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_
