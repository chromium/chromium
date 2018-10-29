// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/aura/overscroll_window_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/base/layout.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_png_rep.h"

namespace content {
namespace {

// Returns true if the entry's URL or any of the URLs in entry's redirect chain
// match |url|.
bool DoesEntryMatchURL(NavigationEntry* entry, const GURL& url) {
  if (!entry)
    return false;
  if (entry->GetURL() == url)
    return true;
  const std::vector<GURL>& redirect_chain = entry->GetRedirectChain();
  for (auto it = redirect_chain.begin(); it != redirect_chain.end(); it++) {
    if (*it == url)
      return true;
  }
  return false;
}

// Records UMA histogram and also user action for the cancelled overscroll.
void RecordNavigationOverscrollCancelled(NavigationDirection direction,
                                         OverscrollSource source) {
  UMA_HISTOGRAM_ENUMERATION("Overscroll.Cancelled3",
                            GetUmaNavigationType(direction, source),
                            NAVIGATION_TYPE_COUNT);
  if (direction == NavigationDirection::BACK)
    RecordAction(base::UserMetricsAction("Overscroll_Cancelled.Back"));
  else
    RecordAction(base::UserMetricsAction("Overscroll_Cancelled.Forward"));
}

}  // namespace

// Responsible for fading out and deleting the layer of the overlay window.
class OverlayDismissAnimator : public ui::ImplicitAnimationObserver {
 public:
  // Takes ownership of the layer.
  explicit OverlayDismissAnimator(std::unique_ptr<ui::Layer> layer)
      : layer_(std::move(layer)) {
    CHECK(layer_.get());
  }

  // Starts the fadeout animation on the layer. When the animation finishes,
  // the object deletes itself along with the layer.
  void Animate() {
    DCHECK(layer_.get());
    // This makes SetOpacity() animate with default duration (which could be
    // zero, e.g. when running tests).
    ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
    settings.AddObserver(this);
    layer_->SetOpacity(0);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(OverlayDismissAnimator);
};

OverscrollNavigationOverlay::OverscrollNavigationOverlay(
    WebContentsImpl* web_contents,
    aura::Window* web_contents_window)
    : direction_(NavigationDirection::NONE),
      web_contents_(web_contents),
      loading_complete_(false),
      received_paint_update_(false),
      owa_(new OverscrollWindowAnimation(this)),
      web_contents_window_(web_contents_window) {
}

OverscrollNavigationOverlay::~OverscrollNavigationOverlay() {
  aura::Window* event_window = GetMainWindow();
  if (owa_->is_active() && event_window)
    event_window->ReleaseCapture();
}

void OverscrollNavigationOverlay::StartObserving() {
  loading_complete_ = false;
  received_paint_update_ = false;
  Observe(web_contents_);

  // Assumes the navigation has been initiated.
  NavigationEntry* pending_entry =
      web_contents_->GetController().GetPendingEntry();
  // Save url of the pending entry to identify when it loads and paints later.
  // Under some circumstances navigation can leave a null pending entry -
  // see comments in NavigationControllerImpl::NavigateToPendingEntry().
  pending_entry_url_ = pending_entry ? pending_entry->GetURL() : GURL();
}

void OverscrollNavigationOverlay::StopObservingIfDone() {
  // Normally we dismiss the overlay once we receive a paint update, however
  // for in-page navigations DidFirstVisuallyNonEmptyPaint() does not get
  // called, and we rely on loading_complete_ for those cases.
  // If an overscroll gesture is in progress, then do not destroy the window.
  if (!window_ || !(loading_complete_ || received_paint_update_) ||
      owa_->is_active()) {
    return;
  }

  // OverlayDismissAnimator deletes the dismiss layer and itself when the
  // animation completes.
  std::unique_ptr<ui::Layer> dismiss_layer = window_->AcquireLayer();
  window_.reset();
  (new OverlayDismissAnimator(std::move(dismiss_layer)))->Animate();
  Observe(nullptr);
  received_paint_update_ = false;
  loading_complete_ = false;
}

std::unique_ptr<aura::Window> OverscrollNavigationOverlay::CreateOverlayWindow(
    const gfx::Rect& bounds) {
  UMA_HISTOGRAM_ENUMERATION(
      "Overscroll.Started3",
      GetUmaNavigationType(direction_, owa_->overscroll_source()),
      NAVIGATION_TYPE_COUNT);
  OverscrollWindowDelegate* overscroll_delegate = new OverscrollWindowDelegate(
      owa_.get(), GetImageForDirection(direction_));
  std::unique_ptr<aura::Window> window(new aura::Window(overscroll_delegate));
  window->set_owned_by_parent(false);
  window->SetTransparent(true);
  window->Init(ui::LAYER_TEXTURED);
  window->layer()->SetMasksToBounds(false);
  window->SetName("OverscrollOverlay");
  web_contents_window_->AddChild(window.get());
  aura::Window* event_window = GetMainWindow();
  if (direction_ == NavigationDirection::FORWARD)
    web_contents_window_->StackChildAbove(window.get(), event_window);
  else
    web_contents_window_->StackChildBelow(window.get(), event_window);
  window->SetBounds(bounds);
  // Set capture on the window that is receiving the overscroll events so that
  // trackpad scroll gestures keep targetting it even if the mouse pointer moves
  // off its bounds.
  event_window->SetCapture();
  window->Show();
  return window;
}

const gfx::Image OverscrollNavigationOverlay::GetImageForDirection(
    NavigationDirection direction) const {
  const NavigationControllerImpl& controller = web_contents_->GetController();
  const NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      controller.GetEntryAtOffset(
          direction == NavigationDirection::FORWARD ? 1 : -1));

  if (entry && entry->screenshot().get()) {
    std::vector<gfx::ImagePNGRep> image_reps;
    image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(), 1.0f));
    return gfx::Image(image_reps);
  }
  return gfx::Image();
}

std::unique_ptr<aura::Window> OverscrollNavigationOverlay::CreateFrontWindow(
    const gfx::Rect& bounds) {
  if (!web_contents_->GetController().CanGoForward())
    return nullptr;
  direction_ = NavigationDirection::FORWARD;
  return CreateOverlayWindow(bounds);
}

std::unique_ptr<aura::Window> OverscrollNavigationOverlay::CreateBackWindow(
    const gfx::Rect& bounds) {
  if (!web_contents_->GetController().CanGoBack())
    return nullptr;
  direction_ = NavigationDirection::BACK;
  return CreateOverlayWindow(bounds);
}

aura::Window* OverscrollNavigationOverlay::GetMainWindow() const {
  if (window_)
    return window_.get();
  return web_contents_->IsBeingDestroyed()
             ? nullptr
             : web_contents_->GetContentNativeView();
}

void OverscrollNavigationOverlay::OnOverscrollCompleting() {
  aura::Window* main_window = GetMainWindow();
  if (!main_window)
    return;
  main_window->ReleaseCapture();
}

void OverscrollNavigationOverlay::OnOverscrollCompleted(
    std::unique_ptr<aura::Window> window) {
  DCHECK_NE(direction_, NavigationDirection::NONE);
  aura::Window* main_window = GetMainWindow();
  if (!main_window) {
    RecordNavigationOverscrollCancelled(direction_, owa_->overscroll_source());
    return;
  }

  main_window->SetTransform(gfx::Transform());
  window_ = std::move(window);
  // Make sure the window is in its default position.
  window_->SetBounds(gfx::Rect(web_contents_window_->bounds().size()));
  window_->SetTransform(gfx::Transform());
  // Make sure the overlay window is on top.
  web_contents_window_->StackChildAtTop(window_.get());

  // Make sure we can navigate first, as other factors can trigger a navigation
  // during an overscroll gesture and navigating without history produces a
  // crash.
  bool navigated = false;
  if (direction_ == NavigationDirection::FORWARD &&
      web_contents_->GetController().CanGoForward()) {
    web_contents_->GetController().GoForward();
    navigated = true;
  } else if (direction_ == NavigationDirection::BACK &&
      web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    navigated = true;
  } else {
    // We need to dismiss the overlay without navigating as soon as the
    // overscroll finishes.
    RecordNavigationOverscrollCancelled(direction_, owa_->overscroll_source());
    loading_complete_ = true;
  }

  if (navigated) {
    UMA_HISTOGRAM_ENUMERATION(
        "Overscroll.Navigated3",
        GetUmaNavigationType(direction_, owa_->overscroll_source()),
        NAVIGATION_TYPE_COUNT);
    if (direction_ == NavigationDirection::BACK)
      RecordAction(base::UserMetricsAction("Overscroll_Navigated.Back"));
    else
      RecordAction(base::UserMetricsAction("Overscroll_Navigated.Forward"));
    StartObserving();
  }

  direction_ = NavigationDirection::NONE;
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::OnOverscrollCancelled() {
  RecordNavigationOverscrollCancelled(direction_, owa_->overscroll_source());
  aura::Window* main_window = GetMainWindow();
  if (!main_window)
    return;
  main_window->ReleaseCapture();
  direction_ = NavigationDirection::NONE;
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::DidFirstVisuallyNonEmptyPaint() {
  NavigationEntry* visible_entry =
      web_contents_->GetController().GetVisibleEntry();
  if (pending_entry_url_.is_empty() ||
      DoesEntryMatchURL(visible_entry, pending_entry_url_)) {
    received_paint_update_ = true;
    StopObservingIfDone();
  }
}

void OverscrollNavigationOverlay::DidStopLoading() {
  // Don't compare URLs in this case - it's possible they won't match if
  // a gesture-nav initiated navigation was interrupted by some other in-site
  // navigation (e.g., from a script, or from a bookmark).
  loading_complete_ = true;
  StopObservingIfDone();
}

}  // namespace content
