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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT WebIDBCursor {
 public:
  virtual ~WebIDBCursor() = default;

  // Used to implement IDBCursor.advance().
  virtual void Advance(unsigned long count, WebIDBCallbacks*) = 0;

  // Used to implement IDBCursor.continue() and IDBCursor.continuePrimaryKey().
  //
  // The key and primary key are null when they are not supplied by the
  // application. When both arguments are null, the cursor advances by one
  // entry.
  //
  // The keys pointed to by WebIDBKeyView are only guaranteed to be alive for
  // the duration of the call.
  virtual void CursorContinue(WebIDBKeyView,
                              WebIDBKeyView primary_key,
                              WebIDBCallbacks*) = 0;

  // Called after a cursor request's success handler is executed.
  //
  // This is only used by the cursor prefetching logic, and does not result in
  // an IPC.
  virtual void PostSuccessHandlerCallback() = 0;

 protected:
  WebIDBCursor() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CURSOR_H_
