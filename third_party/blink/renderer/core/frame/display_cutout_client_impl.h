// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

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
  DisplayCutoutClientImpl(const DisplayCutoutClientImpl&) = delete;
  DisplayCutoutClientImpl& operator=(const DisplayCutoutClientImpl&) = delete;

  // Notify the renderer that the safe areas have changed.
  void SetSafeArea(const gfx::Insets& safe_area) override;

  // Fires a DevTools Audits issue if `frame` uses viewport-fit=cover, the
  // applied top safe-area inset is > 0, no CSS consumes
  // env(safe-area-inset-top), and DOM content is likely occluded by the
  // inset. No-op unless a DevTools audits agent is attached to the frame.
  static void MaybeReportMissingSafeAreaInsetTopIssue(
      LocalFrame* frame,
      const gfx::Insets& safe_area);

  void Trace(Visitor*) const;

 private:
  Member<LocalFrame> frame_;

  // Whether this frame has already reported the DevTools issue for a page
  // using viewport-fit=cover that receives a non-zero top safe-area inset
  // but never consumes env(safe-area-inset-top).
  bool has_reported_missing_safe_area_inset_top_issue_ = false;

  HeapMojoAssociatedReceiver<mojom::blink::DisplayCutoutClient,
                             DisplayCutoutClientImpl>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
