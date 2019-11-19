// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LocalFrame;

// Mojo interface to set CSS environment variables for display cutout.
class CORE_EXPORT DisplayCutoutClientImpl final
    : public GarbageCollected<DisplayCutoutClientImpl>,
      public mojom::blink::DisplayCutoutClient {
 public:
  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>);

  DisplayCutoutClientImpl(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>);

  // Notify the renderer that the safe areas have changed.
  void SetSafeArea(mojom::blink::DisplayCutoutSafeAreaPtr safe_area) override;

  void Trace(Visitor*);

 private:
  Member<LocalFrame> frame_;

  mojo::AssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCutoutClientImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
