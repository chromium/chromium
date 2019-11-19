// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_

#include <memory>
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class PostMessageOptions;
class ScriptState;

class MODULES_EXPORT ServiceWorkerClient : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ServiceWorkerClient* Create(
      const mojom::blink::ServiceWorkerClientInfo&);

  explicit ServiceWorkerClient(const mojom::blink::ServiceWorkerClientInfo&);
  ~ServiceWorkerClient() override;

  // Client.idl
  String url() const { return url_; }
  String type() const;
  String frameType(ScriptState*) const;
  String id() const { return uuid_; }
  String lifecycleState() const;
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   HeapVector<ScriptValue>& transfer,
                   ExceptionState&);
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   const PostMessageOptions*,
                   ExceptionState&);

 protected:
  String Uuid() const { return uuid_; }

 private:
  const String uuid_;
  const String url_;
  const mojom::ServiceWorkerClientType type_;
  const network::mojom::RequestContextFrameType frame_type_;
  const mojom::ServiceWorkerClientLifecycleState lifecycle_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_
