// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/viewport_data.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom-blink.h"

namespace blink {

ViewportData::ViewportData(Document& document)
    : document_(document),
      display_cutout_host_(document_->GetExecutionContext()) {}

void ViewportData::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(display_cutout_host_);
}

void ViewportData::Shutdown() {
  display_cutout_host_.reset();
}

bool ViewportData::ShouldMergeWithLegacyDescription(
    ViewportDescription::Type origin) const {
  return document_->GetSettings() &&
         document_->GetSettings()->GetViewportMetaMergeContentQuirk() &&
         legacy_viewport_description_.IsMetaViewportType() &&
         legacy_viewport_description_.type == origin;
}

void ViewportData::SetViewportDescription(
    const ViewportDescription& viewport_description) {
  if (viewport_description.IsLegacyViewportType()) {
    if (viewport_description == legacy_viewport_description_)
      return;
    legacy_viewport_description_ = viewport_description;
  } else {
    if (viewport_description == viewport_description_)
      return;
    viewport_description_ = viewport_description;

    // Store the UA specified width to be used as the default "fallback" width.
    // i.e. the width to use if the author doesn't specify a layout width.
    if (!viewport_description.IsSpecifiedByAuthor())
      viewport_default_min_width_ = viewport_description.min_width;
  }

  UpdateViewportDescription();
}

ViewportDescription ViewportData::GetViewportDescription() const {
  ViewportDescription applied_viewport_description = viewport_description_;
  bool viewport_meta_enabled =
      document_->GetSettings() &&
      document_->GetSettings()->GetViewportMetaEnabled();
  if (legacy_viewport_description_.type !=
          ViewportDescription::kUserAgentStyleSheet &&
      viewport_meta_enabled)
    applied_viewport_description = legacy_viewport_description_;
  if (ShouldOverrideLegacyDescription(viewport_description_.type))
    applied_viewport_description = viewport_description_;

  // Setting `navigator.virtualKeyboard.overlaysContent` should override the
  // virtual-keyboard mode set from the viewport meta tag.
  if (virtual_keyboard_overlays_content_) {
    applied_viewport_description.virtual_keyboard_mode =
        ui::mojom::blink::VirtualKeyboardMode::kOverlaysContent;
  }

  return applied_viewport_description;
}

void ViewportData::UpdateViewportDescription() {
  if (!document_->GetFrame())
    return;

  // If the viewport_fit has changed we should send this to the browser. We
  // use the legacy viewport description which contains the viewport_fit
  // defined from the layout meta tag.
  mojom::ViewportFit current_viewport_fit =
      GetViewportDescription().GetViewportFit();

  // If we are forcing to expand into the display cutout then we should override
  // the viewport fit value.
  if (force_expand_display_cutout_)
    current_viewport_fit = mojom::ViewportFit::kCoverForcedByUserAgent;

  if (viewport_fit_ != current_viewport_fit) {
    if (AssociatedInterfaceProvider* provider =
            document_->GetFrame()
                ->Client()
                ->GetRemoteNavigationAssociatedInterfaces()) {
      // Bind the mojo interface.
      if (!display_cutout_host_.is_bound()) {
        provider->GetInterface(
            display_cutout_host_.BindNewEndpointAndPassReceiver(
                provider->GetTaskRunner()));
        DCHECK(display_cutout_host_.is_bound());
      }

      // Even though we bind the mojo interface above there still may be cases
      // where this will fail (e.g. unit tests).
      display_cutout_host_->NotifyViewportFitChanged(current_viewport_fit);

      // Track usage of any non-default viewport-fit.
      if (document_->GetFrame()->IsOutermostMainFrame()) {
        if (current_viewport_fit == mojom::blink::ViewportFit::kContain) {
          UseCounter::Count(document_, WebFeature::kViewportFitContain);
        } else if (current_viewport_fit == mojom::blink::ViewportFit::kCover ||
                   current_viewport_fit ==
                       mojom::blink::ViewportFit::kCoverForcedByUserAgent) {
          UseCounter::Count(document_, WebFeature::kViewportFitCover);
          // TODO(https://crbug.com/1482559) remove tracking this union of
          // features after data collected (end of '23)
          UseCounter::Count(document_,
                            WebFeature::kViewportFitCoverOrSafeAreaInsetBottom);
          // TODO(https://crbug.com/1482559#c23) remove this line by end of
          // 2023.
          VLOG(0) << "E2E_Used ViewportFitCover";
        }
      }
    }

    viewport_fit_ = current_viewport_fit;
  }

  if (document_->GetFrame()->IsMainFrame() &&
      document_->GetPage()->GetVisualViewport().IsActiveViewport()) {
    document_->GetPage()->GetChromeClient().DispatchViewportPropertiesDidChange(
        GetViewportDescription());
  }
}

void ViewportData::SetExpandIntoDisplayCutout(bool expand) {
  if (force_expand_display_cutout_ == expand)
    return;

  force_expand_display_cutout_ = expand;
  UpdateViewportDescription();
}

void ViewportData::SetVirtualKeyboardOverlaysContent(bool overlays_content) {
  if (virtual_keyboard_overlays_content_ == overlays_content)
    return;

  virtual_keyboard_overlays_content_ = overlays_content;
  UpdateViewportDescription();
}

}  // namespace blink
