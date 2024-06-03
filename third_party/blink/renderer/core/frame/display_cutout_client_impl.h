// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "ui/gfx/geometry/insets.h"

namespace blink {

class LocalFrame;

// Mojo interface to set CSS environment variables for display cutout.
class CORE_EXPORT DisplayCutoutClientImpl final
    : public GarbageCollected<DisplayCutoutClientImpl>,
      public Supplement<LocalFrame>,
      public mojom::blink::DisplayCutoutClient {
 public:
  static const char kSupplementName[];

  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>);
  static DisplayCutoutClientImpl* From(LocalFrame* frame);

  DisplayCutoutClientImpl(
      LocalFrame&,
      mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>);
  DisplayCutoutClientImpl(const DisplayCutoutClientImpl&) = delete;
  DisplayCutoutClientImpl& operator=(const DisplayCutoutClientImpl&) = delete;

  // Notify the renderer that the safe areas have changed.
  void SetSafeArea(const gfx::Insets& safe_area) override;

  // Update the CSS safe-area-inset* attribute in the document based on the
  // stored safe_area_insets and the given |browser_controls|'s visible height.
  // The new safe-area-inset-bottom will not be applied to the CSS environment
  // if the value after calculation is the same as this previous call, unless
  // |force_update| is true.
  void UpdateSafeAreaInsetWithBrowserControls(
      Document* document,
      const BrowserControls& browser_controls,
      bool force_update = false);

 private:
  // Set the save area insets in the current page.
  gfx::Insets safe_area_insets_;

  // Last set safe-area-insets-bottom CSS environment variable.
  float last_set_safe_are_bottom_insets_;

  GC_PLUGIN_IGNORE("https://crbug.com/1381979")
  mojo::AssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DISPLAY_CUTOUT_CLIENT_IMPL_H_
