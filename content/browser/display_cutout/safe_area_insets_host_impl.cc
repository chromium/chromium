// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/safe_area_insets_host_impl.h"

#include "content/browser/display_cutout/display_cutout_constants.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

// DocumentUserData stored inside each RenderFrameHost indicating the
// viewport-fit value for that document.
class CONTENT_EXPORT SafeAreaUserData
    : public DocumentUserData<SafeAreaUserData> {
 public:
  ~SafeAreaUserData() override = default;

  void set_viewport_fit(blink::mojom::ViewportFit value) { value_ = value; }
  blink::mojom::ViewportFit viewport_fit() { return value_; }

  void set_safe_area_constraint(bool value) {
    has_safe_area_constraint_ = value;
  }
  bool has_safe_area_constraint() { return has_safe_area_constraint_; }

 private:
  explicit SafeAreaUserData(RenderFrameHost* rfh)
      : DocumentUserData<SafeAreaUserData>(rfh) {}

  // The viewport-fit value known by blink, or kAuto.
  blink::mojom::ViewportFit value_ = blink::mojom::ViewportFit::kAuto;
  bool has_safe_area_constraint_ = false;

  // NOTE: Do not add data members without updating SetViewportFitValue.
  // This is because data does not need to be stored if it consists purely
  // of default values. If new data is added then SetViewportFitValue must
  // check if the new data also has a default value before skipping storage.

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(SafeAreaUserData);

SafeAreaInsetsHostImpl::SafeAreaInsetsHostImpl(WebContentsImpl* web_contents)
    : SafeAreaInsetsHost(web_contents) {}

SafeAreaInsetsHostImpl::~SafeAreaInsetsHostImpl() = default;

void SafeAreaInsetsHostImpl::DidAcquireFullscreen(RenderFrameHost* rfh) {
  fullscreen_rfh_ = static_cast<RenderFrameHostImpl*>(rfh)->GetWeakPtr();
  ClearSafeAreaInsetsForActiveFrame();
  MaybeActiveRenderFrameHostChanged();
}

void SafeAreaInsetsHostImpl::DidExitFullscreen() {
  ClearSafeAreaInsetsForActiveFrame();
  fullscreen_rfh_.reset();
  MaybeActiveRenderFrameHostChanged();
}

void SafeAreaInsetsHostImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation is committed and we are not a same-document
  // navigation then set the current Render Frame Host and its value.
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
    DCHECK(rfh);
    current_rfh_ = static_cast<RenderFrameHostImpl*>(rfh)->GetWeakPtr();

    blink::mojom::DisplayMode mode = web_contents_impl_->GetDisplayMode();
    if (mode == blink::mojom::DisplayMode::kFullscreen &&
        active_render_frame_host() != current_rfh_.get()) {
      ClearSafeAreaInsetsForActiveFrame();
    }

    MaybeActiveRenderFrameHostChanged();
  }
}

void SafeAreaInsetsHostImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  RenderFrameHostImpl* rfh = active_render_frame_host();
  if (rfh) {
    // Skip sending the safe area to frame if the values match the latest sent
    // values.
    if (insets != insets_) {
      MaybeSendSafeAreaToFrame(rfh, insets);
    }
  }
  insets_ = insets;
}

void SafeAreaInsetsHostImpl::ViewportFitChangedForFrame(
    RenderFrameHost* rfh,
    blink::mojom::ViewportFit value) {
  DCHECK(rfh);
  SetViewportFitValue(rfh, value);

  // If we are the active `RenderFrameHost` frame then notify
  // WebContentsObservers about the new value.
  if (rfh == active_render_frame_host()) {
    MaybeActiveRenderFrameHostChanged();
  }
}

void SafeAreaInsetsHostImpl::ComplexSafeAreaConstraintChangedForFrame(
    RenderFrameHost* rfh,
    bool has_constraint) {
  DCHECK(rfh);
  SetSafeAreaConstraintValue(rfh, has_constraint);

  if (rfh == active_render_frame_host()) {
    active_has_constraint_ = has_constraint;
    web_contents_impl_->NotifySafeAreaConstraintChanged(has_constraint);
  }
}

void SafeAreaInsetsHostImpl::MaybeActiveRenderFrameHostChanged() {
  base::WeakPtr<RenderFrameHostImpl> new_active_rfh =
      fullscreen_rfh_ ? fullscreen_rfh_ : current_rfh_;
  active_rfh_ = new_active_rfh;

  blink::mojom::ViewportFit new_viewport_fit =
      GetValueOrDefault(active_render_frame_host());
  if (new_viewport_fit != active_viewport_fit_) {
    active_viewport_fit_ = new_viewport_fit;
    web_contents_impl_->NotifyViewportFitChanged(new_viewport_fit);
  }
  bool new_has_constraint =
      GetSafeAreaConstraintOrDefault(active_render_frame_host());
  if (new_has_constraint != active_has_constraint_) {
    active_has_constraint_ = new_has_constraint;
    web_contents_impl_->NotifySafeAreaConstraintChanged(new_has_constraint);
  }

  // Update Blink so its document displays with the current insets.
  if (new_active_rfh) {
    MaybeSendSafeAreaToFrame(new_active_rfh.get(), insets_);
  }
}

void SafeAreaInsetsHostImpl::ClearSafeAreaInsetsForActiveFrame() {
  if (active_render_frame_host()) {
    MaybeSendSafeAreaToFrame(active_render_frame_host(), gfx::Insets());
  }
}

// TODO (crbug.com/376573458): Improve logic further to track per frame, such
// that the optimization isn't lost when one non-zero inset is sent.
void SafeAreaInsetsHostImpl::MaybeSendSafeAreaToFrame(RenderFrameHost* rfh,
                                                      gfx::Insets insets) {
  bool are_zero_insets = (insets == kZeroInsets);
  if (are_zero_insets && !has_sent_non_zero_insets_) {
    return;
  }
  if (!are_zero_insets) {
    has_sent_non_zero_insets_ = true;
  }
  SendSafeAreaToFrame(rfh, insets);
}

blink::mojom::ViewportFit SafeAreaInsetsHostImpl::GetValueOrDefault(
    RenderFrameHost* rfh) const {
  // The active RenderFrameHost can be null in some cases, such as if fullscreen
  // mode is exited before the navigation finishes.
  if (rfh) {
    SafeAreaUserData* data = SafeAreaUserData::GetForCurrentDocument(rfh);
    if (data) {
      return data->viewport_fit();
    }
  }
  return blink::mojom::ViewportFit::kAuto;
}

void SafeAreaInsetsHostImpl::SetViewportFitValue(
    RenderFrameHost* rfh,
    blink::mojom::ViewportFit value) {
  SafeAreaUserData::GetOrCreateForCurrentDocument(rfh)->set_viewport_fit(value);
}

void SafeAreaInsetsHostImpl::SetSafeAreaConstraintValue(RenderFrameHost* rfh,
                                                        bool has_constraint) {
  SafeAreaUserData::GetOrCreateForCurrentDocument(rfh)
      ->set_safe_area_constraint(has_constraint);
}

bool SafeAreaInsetsHostImpl::GetSafeAreaConstraintOrDefault(
    RenderFrameHost* rfh) const {
  if (!rfh) {
    return false;
  }
  SafeAreaUserData* data = SafeAreaUserData::GetForCurrentDocument(rfh);
  return data != nullptr && data->has_safe_area_constraint();
}

}  // namespace content
