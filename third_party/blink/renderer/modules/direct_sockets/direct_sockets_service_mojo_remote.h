// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_MOJO_REMOTE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_MOJO_REMOTE_H_

#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// The connecting link between the renderer and the browser.
class MODULES_EXPORT DirectSocketsServiceMojoRemote
    : public GarbageCollected<DirectSocketsServiceMojoRemote> {
 public:
  static DirectSocketsServiceMojoRemote* Create(
      ExecutionContext*,
      base::OnceClosure disconnect_handler);

  explicit DirectSocketsServiceMojoRemote(ExecutionContext*);
  ~DirectSocketsServiceMojoRemote();

  HeapMojoRemote<blink::mojom::blink::DirectSocketsService>& get() {
    return service_;
  }

  void Close();

  void Trace(Visitor*) const;

 private:
  HeapMojoRemote<blink::mojom::blink::DirectSocketsService> service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_MOJO_REMOTE_H_
