// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_JAVASCRIPT_CALL_STACK_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_JAVASCRIPT_CALL_STACK_GENERATOR_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/call_stack_generator/call_stack_generator.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class CONTROLLER_EXPORT JavaScriptCallStackGenerator
    : public mojom::blink::CallStackGenerator {
 public:
  static void Bind(
      mojo::PendingReceiver<mojom::blink::CallStackGenerator> receiver);
  void CollectJavaScriptCallStack(
      CollectJavaScriptCallStackCallback callback) override;
  void HandleCallStackCollected(
      const String& call_stack,
      const std::optional<LocalFrameToken> frame_token);

 private:
  void InterruptIsolateAndCollectCallStack(v8::Isolate* isolate);

  mojo::Receiver<mojom::blink::CallStackGenerator> receiver_{this};
  CollectJavaScriptCallStackCallback callback_;
  bool call_stack_collected_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_JAVASCRIPT_CALL_STACK_GENERATOR_H_
