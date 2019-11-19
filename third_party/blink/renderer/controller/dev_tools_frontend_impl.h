/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_DEV_TOOLS_FRONTEND_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_DEV_TOOLS_FRONTEND_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/devtools/devtools_frontend.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/inspector_frontend_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DevToolsHost;
class LocalFrame;

// This class lives as long as a frame (being a supplement), or until
// it's host (mojom.DevToolsFrontendHost) is destroyed.
class DevToolsFrontendImpl final
    : public GarbageCollected<DevToolsFrontendImpl>,
      public Supplement<LocalFrame>,
      public mojom::blink::DevToolsFrontend,
      public InspectorFrontendClient {
  USING_GARBAGE_COLLECTED_MIXIN(DevToolsFrontendImpl);

 public:
  static const char kSupplementName[];

  static void BindMojoRequest(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::DevToolsFrontend>);
  static DevToolsFrontendImpl* From(LocalFrame*);

  DevToolsFrontendImpl(
      LocalFrame&,
      mojo::PendingAssociatedReceiver<mojom::blink::DevToolsFrontend>);
  ~DevToolsFrontendImpl() override;
  void DidClearWindowObject();
  void Trace(blink::Visitor*) override;

 private:
  void DestroyOnHostGone();

  // mojom::blink::DevToolsFrontend implementation.
  void SetupDevToolsFrontend(
      const String& api_script,
      mojo::PendingAssociatedRemote<mojom::blink::DevToolsFrontendHost>)
      override;
  void SetupDevToolsExtensionAPI(const String& extension_api) override;

  // InspectorFrontendClient implementation.
  void SendMessageToEmbedder(const String&) override;

  Member<DevToolsHost> devtools_host_;
  String api_script_;
  mojo::AssociatedRemote<mojom::blink::DevToolsFrontendHost> host_;
  mojo::AssociatedReceiver<mojom::blink::DevToolsFrontend> receiver_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendImpl);
};

}  // namespace blink

#endif
