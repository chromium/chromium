// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENTS_H_

#include "third_party/blink/renderer/modules/service_worker/client_query_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ScriptPromise;
class ScriptState;

class ServiceWorkerClients final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ServiceWorkerClients* Create();

  ServiceWorkerClients();

  // Clients.idl
  ScriptPromise get(ScriptState*, const String& id);
  ScriptPromise matchAll(ScriptState*, const ClientQueryOptions*);
  ScriptPromise openWindow(ScriptState*, const String& url);
  ScriptPromise claim(ScriptState*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENTS_H_
