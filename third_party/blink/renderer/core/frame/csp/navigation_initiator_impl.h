// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_NAVIGATION_INITIATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_NAVIGATION_INITIATOR_IMPL_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;

class NavigationInitiatorImpl
    : public GarbageCollected<NavigationInitiatorImpl>,
      public mojom::blink::NavigationInitiator {
  USING_PRE_FINALIZER(NavigationInitiatorImpl, Dispose);

 public:
  explicit NavigationInitiatorImpl(Document& document);
  void Trace(Visitor* visitor);

  // mojom::blink::NavigationInitiator override:
  void SendViolationReport(
      mojom::blink::CSPViolationParamsPtr violation_params) override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::blink::NavigationInitiator> receiver) {
    navigation_initiator_receivers_.Add(this, std::move(receiver));
  }

 private:
  void Dispose();

  // A list of all the navigation_initiator receivers owned by the owner
  // document. Used to report CSP violations that result from CSP blocking
  // navigation requests that were initiated by the owner document.
  mojo::ReceiverSet<mojom::blink::NavigationInitiator>
      navigation_initiator_receivers_;

  Member<Document> document_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_NAVIGATION_INITIATOR_IMPL_H_
