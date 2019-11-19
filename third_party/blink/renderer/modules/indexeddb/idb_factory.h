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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_FACTORY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;

class MODULES_EXPORT IDBFactory final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  IDBFactory();
  IDBFactory(std::unique_ptr<WebIDBFactory>);

  // Implement the IDBFactory IDL
  IDBOpenDBRequest* open(ScriptState*, const String& name, ExceptionState&);
  IDBOpenDBRequest* open(ScriptState*,
                         const String& name,
                         uint64_t version,
                         ExceptionState&);
  IDBOpenDBRequest* deleteDatabase(ScriptState*,
                                   const String& name,
                                   ExceptionState&);
  int16_t cmp(ScriptState*,
              const ScriptValue& first,
              const ScriptValue& second,
              ExceptionState&);

  // These are not exposed to the web applications and only used by DevTools.
  IDBRequest* GetDatabaseNames(ScriptState*, ExceptionState&);
  IDBOpenDBRequest* CloseConnectionsAndDeleteDatabase(ScriptState*,
                                                      const String& name,
                                                      ExceptionState&);

  ScriptPromise GetDatabaseInfo(ScriptState*, ExceptionState&);

 private:
  WebIDBFactory* GetFactory(ExecutionContext* execution_context);

  IDBOpenDBRequest* OpenInternal(ScriptState*,
                                 const String& name,
                                 int64_t version,
                                 ExceptionState&);

  IDBOpenDBRequest* DeleteDatabaseInternal(ScriptState*,
                                           const String& name,
                                           ExceptionState&,
                                           bool);

  bool AllowIndexedDB(ScriptState* script_state);
  bool CachedAllowIndexedDB(ScriptState* script_state);

  std::unique_ptr<WebIDBFactory> web_idb_factory_;
  base::Optional<bool> cached_allowed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_FACTORY_H_
