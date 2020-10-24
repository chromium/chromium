// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_impl.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_system_notification_observer.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key used to associate the stored FullscreenControllerContainer.
const char kFullscreenControllerContainerUserDataKey[] =
    "fullscreen_controller_container";

// Container that manages storing the FullscreenControllerImpl to a UserData
// key.
//
// TODO(crbug.com/790886): Remove and subclass FullscreenController to
// BrowserUserData when the flag is turned on by default.
class FullscreenControllerContainer : public base::SupportsUserData::Data {
 public:
  FullscreenControllerContainer(Browser* browser)
      : fullscreen_controller_(
            std::make_unique<FullscreenControllerImpl>(browser)) {}
  FullscreenControllerContainer()
      : fullscreen_controller_(
            std::make_unique<FullscreenControllerImpl>(nullptr)) {}
  ~FullscreenControllerContainer() override;

  FullscreenControllerContainer(const FullscreenControllerContainer&) = delete;
  FullscreenControllerContainer& operator=(
      const FullscreenControllerContainer&) = delete;

  FullscreenControllerContainer(FullscreenControllerContainer&&) = default;
  FullscreenControllerContainer& operator=(FullscreenControllerContainer&&) =
      default;

  static FullscreenControllerContainer* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static FullscreenControllerContainer* GetForBrowser(Browser* browser);

  FullscreenController* GetFullscreenController();

 private:
  std::unique_ptr<FullscreenControllerImpl> fullscreen_controller_;
};

FullscreenControllerContainer::~FullscreenControllerContainer() {
  fullscreen_controller_.reset();
}

FullscreenControllerContainer*
FullscreenControllerContainer::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  ChromeBrowserState* original_browser_state = static_cast<ChromeBrowserState*>(
      GetBrowserStateRedirectedInIncognito(browser_state));
  FullscreenControllerContainer* fullscreen_controller_container =
      static_cast<FullscreenControllerContainer*>(
          original_browser_state->GetUserData(
              kFullscreenControllerContainerUserDataKey));
  if (!fullscreen_controller_container) {
    fullscreen_controller_container = new FullscreenControllerContainer;
    original_browser_state->SetUserData(
        kFullscreenControllerContainerUserDataKey,
        base::WrapUnique(fullscreen_controller_container));
  }
  return fullscreen_controller_container;
}

FullscreenControllerContainer* FullscreenControllerContainer::GetForBrowser(
    Browser* browser) {
  DCHECK(fullscreen::features::ShouldScopeFullscreenControllerToBrowser());
  FullscreenControllerContainer* fullscreen_controller_container =
      static_cast<FullscreenControllerContainer*>(
          browser->GetUserData(kFullscreenControllerContainerUserDataKey));
  if (!fullscreen_controller_container) {
    fullscreen_controller_container =
        new FullscreenControllerContainer(browser);
    browser->SetUserData(kFullscreenControllerContainerUserDataKey,
                         base::WrapUnique(fullscreen_controller_container));
  }
  return fullscreen_controller_container;
}

FullscreenController* FullscreenControllerContainer::GetFullscreenController() {
  return static_cast<FullscreenController*>(fullscreen_controller_.get());
}

}  // namespace

// static
FullscreenController* FullscreenController::FromBrowser(Browser* browser) {
  return FullscreenControllerContainer::GetForBrowser(browser)
      ->GetFullscreenController();
}

FullscreenController* FullscreenController::FromBrowserState(
    ChromeBrowserState* browser_state) {
  return FullscreenControllerContainer::GetForBrowserState(browser_state)
      ->GetFullscreenController();
}

FullscreenControllerImpl::FullscreenControllerImpl(Browser* browser)
    : broadcaster_([[ChromeBroadcaster alloc] init]),
      mediator_(this, &model_),
      web_state_list_observer_(this, &model_, &mediator_),
      fullscreen_browser_observer_(&web_state_list_observer_, browser),
      bridge_([[ChromeBroadcastOberverBridge alloc] initWithObserver:&model_]),
      notification_observer_([[FullscreenSystemNotificationObserver alloc]
          initWithController:this
                    mediator:&mediator_]) {
  DCHECK(broadcaster_);
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewSize:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewContentSize:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewContentInset:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewIsZooming:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewIsDragging:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastCollapsedToolbarHeight:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastExpandedToolbarHeight:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastBottomToolbarHeight:)];
  if (!fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
    ios::GetChromeBrowserProvider()
        ->GetFullscreenProvider()
        ->InitializeFullscreen(this);
  }
}

FullscreenControllerImpl::~FullscreenControllerImpl() {
  mediator_.Disconnect();
  web_state_list_observer_.Disconnect();
  [notification_observer_ disconnect];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewSize:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewContentSize:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewContentInset:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsZooming:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsDragging:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastCollapsedToolbarHeight:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastExpandedToolbarHeight:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastBottomToolbarHeight:)];
}

ChromeBroadcaster* FullscreenControllerImpl::broadcaster() {
  return broadcaster_;
}

void FullscreenControllerImpl::SetWebStateList(WebStateList* web_state_list) {
  web_state_list_observer_.SetWebStateList(web_state_list);
}

const WebStateList* FullscreenControllerImpl::GetWebStateList() const {
  return web_state_list_observer_.GetWebStateList();
}

WebStateList* FullscreenControllerImpl::GetWebStateList() {
  return web_state_list_observer_.GetWebStateList();
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
  return model_.enabled();
}

void FullscreenControllerImpl::IncrementDisabledCounter() {
  model_.IncrementDisabledCounter();
}

void FullscreenControllerImpl::DecrementDisabledCounter() {
  model_.DecrementDisabledCounter();
}

bool FullscreenControllerImpl::ResizesScrollView() const {
  return model_.ResizesScrollView();
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedBegin() {
  mediator_.SetIsBrowserTraitCollectionUpdating(true);
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedEnd() {
  mediator_.SetIsBrowserTraitCollectionUpdating(false);
}

CGFloat FullscreenControllerImpl::GetProgress() const {
  return model_.progress();
}

UIEdgeInsets FullscreenControllerImpl::GetMinViewportInsets() const {
  return model_.min_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetMaxViewportInsets() const {
  return model_.max_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetCurrentViewportInsets() const {
  return model_.current_toolbar_insets();
}

void FullscreenControllerImpl::EnterFullscreen() {
  mediator_.EnterFullscreen();
}

void FullscreenControllerImpl::ExitFullscreen() {
  mediator_.ExitFullscreen();
}

void FullscreenControllerImpl::ResizeHorizontalViewport() {
  // TODO(crbug.com/1114054) this hack temporarily force change webview's
  // width insets to trigger a width recomputation of its content. It will cause
  // two relayouts.
  mediator_.ResizeHorizontalInsets();
}
