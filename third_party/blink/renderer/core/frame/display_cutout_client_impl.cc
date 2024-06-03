// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

// static
const char DisplayCutoutClientImpl::kSupplementName[] =
    "DisplayCutoutClientImpl";

DisplayCutoutClientImpl::DisplayCutoutClientImpl(
    LocalFrame& frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver)
    : Supplement<LocalFrame>(frame), receiver_(this, std::move(receiver)) {}

// static
void DisplayCutoutClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>
        receiver) {
  if (!frame)
    return;
  frame->ProvideSupplement(MakeGarbageCollected<DisplayCutoutClientImpl>(
      *frame, std::move(receiver)));
}

// static
DisplayCutoutClientImpl* DisplayCutoutClientImpl::From(LocalFrame* frame) {
  return Supplement<LocalFrame>::From<DisplayCutoutClientImpl>(frame);
}

void DisplayCutoutClientImpl::UpdateSafeAreaInsetWithBrowserControls(
    Document* document,
    const BrowserControls& browser_controls,
    bool force_update) {
  if (!RuntimeEnabledFeatures::DynamicSafeAreaInsetsEnabled()) {
    return;
  }

  DCHECK(document);

  // Adjust the top / left / right is not needed, since they are set when
  // display insets was received at |SetSafeArea()|.
  int inset_bottom = safe_area_insets_.bottom();
  int bottom_controls_full_height = browser_controls.BottomHeight();
  float control_ratio = browser_controls.BottomShownRatio();
  float dip_scale = document->GetPage()->GetVisualViewport().ScaleFromDIP();

  // As control_ratio decrease, safe_area_inset_bottom will be added to the web
  // page to keep the bottom element out from the display cutout area.
  float safe_area_inset_bottom =
      std::max(0.f, inset_bottom - control_ratio * bottom_controls_full_height /
                                       dip_scale);
  if (force_update ||
      safe_area_inset_bottom != last_set_safe_are_bottom_insets_) {
    last_set_safe_are_bottom_insets_ = safe_area_inset_bottom;
    DocumentStyleEnvironmentVariables& vars =
        document->GetStyleEngine().EnsureEnvironmentVariables();
    vars.SetVariable(
        UADefinedVariable::kSafeAreaInsetBottom,
        StyleEnvironmentVariables::FormatPx(safe_area_inset_bottom));
  }
}

void DisplayCutoutClientImpl::SetSafeArea(const gfx::Insets& safe_area) {
  if (safe_area_insets_ == safe_area) {
    return;
  }

  safe_area_insets_ = safe_area;
  last_set_safe_are_bottom_insets_ = safe_area_insets_.bottom();

  // Apply the safe area only if the insets has changed. Safe area can be
  // override by browser controls positions in the page.
  DocumentStyleEnvironmentVariables& vars = GetSupplementable()
                                                ->GetDocument()
                                                ->GetStyleEngine()
                                                .EnsureEnvironmentVariables();
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetTop,
                   StyleEnvironmentVariables::FormatPx(safe_area.top()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetLeft,
                   StyleEnvironmentVariables::FormatPx(safe_area.left()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetBottom,
                   StyleEnvironmentVariables::FormatPx(safe_area.bottom()));
  vars.SetVariable(UADefinedVariable::kSafeAreaInsetRight,
                   StyleEnvironmentVariables::FormatPx(safe_area.right()));
}

}  // namespace blink
