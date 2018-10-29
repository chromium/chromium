// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_impl.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_system_notification_observer.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenControllerImpl::FullscreenControllerImpl()
    : FullscreenController(),
      broadcaster_([[ChromeBroadcaster alloc] init]),
      mediator_(this, &model_),
      web_state_list_observer_(this, &model_, &mediator_),
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
  ios::GetChromeBrowserProvider()
      ->GetFullscreenProvider()
      ->InitializeFullscreen(this);
}

FullscreenControllerImpl::~FullscreenControllerImpl() = default;

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

CGFloat FullscreenControllerImpl::GetProgress() const {
  return model_.progress();
}

void FullscreenControllerImpl::EnterFullscreen() {
  mediator_.EnterFullscreen();
}

void FullscreenControllerImpl::ExitFullscreen() {
  mediator_.ExitFullscreen();
}

void FullscreenControllerImpl::Shutdown() {
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
