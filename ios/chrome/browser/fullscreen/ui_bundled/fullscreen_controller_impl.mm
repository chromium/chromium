// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_impl.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_system_notification_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/web/common/features.h"

// static
std::unique_ptr<FullscreenController> FullscreenController::Create(
    Browser* browser) {
  return base::WrapUnique(new FullscreenControllerImpl(browser));
}

FullscreenControllerImpl::FullscreenControllerImpl(Browser* browser)
    : FullscreenController(browser),
      broadcaster_([[ChromeBroadcaster alloc] init]),
      model_(std::make_unique<FullscreenModel>()),
      mediator_(this, model_.get()),
      web_state_list_observer_(this, model_.get(), &mediator_),
      bridge_(
          [[ChromeBroadcastOberverBridge alloc] initWithObserver:model_.get()]),
      notification_observer_([[FullscreenSystemNotificationObserver alloc]
          initWithController:this
                    mediator:&mediator_]) {
  DCHECK(broadcaster_);
  web_state_list_observer_.SetWebStateList(browser->GetWebStateList());
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewContentSize:)];
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewSize:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsScrolling:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsDragging:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsZooming:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewContentInset:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastContentScrollOffset:)];
  }
  if (!IsRefactorToolbarsSize()) {
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
    [broadcaster_
        addObserver:bridge_
        forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
  }
}

FullscreenControllerImpl::~FullscreenControllerImpl() {
  mediator_.Disconnect();
  web_state_list_observer_.Disconnect();
  [notification_observer_ disconnect];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewContentSize:)];
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewSize:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsScrolling:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsDragging:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsZooming:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewContentInset:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastContentScrollOffset:)];
  }
  if (!IsRefactorToolbarsSize()) {
    [broadcaster_
        removeObserver:bridge_
           forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
    [broadcaster_
        removeObserver:bridge_
           forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
    [broadcaster_
        removeObserver:bridge_
           forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
  }
}

ChromeBroadcaster* FullscreenControllerImpl::broadcaster() {
  return broadcaster_;
}

void FullscreenControllerImpl::AddObserver(
    FullscreenControllerObserver* observer) {
  mediator_.AddObserver(observer);
}

void FullscreenControllerImpl::RemoveObserver(
    FullscreenControllerObserver* observer) {
  mediator_.RemoveObserver(observer);
}

bool FullscreenControllerImpl::IsEnabled() const {
  return model_->enabled();
}

void FullscreenControllerImpl::IncrementDisabledCounter() {
  model_->IncrementDisabledCounter();
}

void FullscreenControllerImpl::DecrementDisabledCounter() {
  model_->DecrementDisabledCounter();
}

bool FullscreenControllerImpl::ResizesScrollView() const {
  return model_->ResizesScrollView();
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedBegin() {
  mediator_.SetIsBrowserTraitCollectionUpdating(true);
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedEnd() {
  mediator_.SetIsBrowserTraitCollectionUpdating(false);
}

CGFloat FullscreenControllerImpl::GetProgress() const {
  return model_->progress();
}

UIEdgeInsets FullscreenControllerImpl::GetMinViewportInsets() const {
  return model_->min_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetMaxViewportInsets() const {
  return model_->max_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetCurrentViewportInsets() const {
  return model_->current_toolbar_insets();
}

void FullscreenControllerImpl::EnterFullscreen() {
  mediator_.EnterFullscreen();
}

// Needs to be cleanup.
void FullscreenControllerImpl::ExitFullscreen() {
  mediator_.ExitFullscreen(FullscreenExitReason::kForcedByCode);
}

void FullscreenControllerImpl::ExitFullscreen(
    FullscreenExitReason fullscreen_exit_reason) {
  mediator_.ExitFullscreen(fullscreen_exit_reason);
}

void FullscreenControllerImpl::ExitFullscreenWithoutAnimation() {
  mediator_.ExitFullscreenWithoutAnimation();
}

bool FullscreenControllerImpl::IsForceFullscreenMode() const {
  return model_->IsForceFullscreenMode();
}

void FullscreenControllerImpl::EnterForceFullscreenMode(
    bool insets_update_enabled) {
  model_->SetForceFullscreenMode(true);
  model_->SetInsetsUpdateEnabled(insets_update_enabled);
  // Disable fullscreen because:
  // - It interfers with the animation when moving the secondary toolbar above
  // the keyboard.
  // - Fullscreen should not resize the toolbar it's above the keyboard.
  IncrementDisabledCounter();
  mediator_.ForceEnterFullscreen();
}

void FullscreenControllerImpl::ExitForceFullscreenMode() {
  if (!IsForceFullscreenMode()) {
    return;
  }
  DecrementDisabledCounter();
  model_->SetForceFullscreenMode(false);
  model_->SetInsetsUpdateEnabled(true);
  mediator_.ExitFullscreenWithoutAnimation();
}

void FullscreenControllerImpl::ResizeHorizontalViewport() {
  // TODO(crbug.com/40143738) this hack temporarily force change webview's
  // width insets to trigger a width recomputation of its content. It will cause
  // two relayouts.
  mediator_.ResizeHorizontalInsets();
}

void FullscreenControllerImpl::SetToolbarsSize(ToolbarsSize* toolbars_size) {
  toolbars_size_ = toolbars_size;
  model_->SetToolbarsSize(toolbars_size);
}

ToolbarsSize* FullscreenControllerImpl::GetToolbarsSize() const {
  return toolbars_size_;
}
