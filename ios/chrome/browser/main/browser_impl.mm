// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserImpl::BrowserImpl(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);

  web_state_list_delegate_ = std::make_unique<BrowserWebStateListDelegate>();
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());

  tab_model_ =
      [[TabModel alloc] initWithSessionService:[SessionServiceIOS sharedService]
                                  browserState:browser_state_
                                  webStateList:web_state_list_.get()];
}

BrowserImpl::BrowserImpl(ios::ChromeBrowserState* browser_state,
                         TabModel* tab_model,
                         std::unique_ptr<WebStateList> web_state_list)
    : browser_state_(browser_state),
      tab_model_(tab_model),
      web_state_list_(std::move(web_state_list)) {
  DCHECK(browser_state_);
  DCHECK(tab_model.webStateList == web_state_list_.get());
}

BrowserImpl::~BrowserImpl() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

ios::ChromeBrowserState* BrowserImpl::GetBrowserState() const {
  return browser_state_;
}

TabModel* BrowserImpl::GetTabModel() const {
  return tab_model_;
}

WebStateList* BrowserImpl::GetWebStateList() const {
  return web_state_list_.get();
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
std::unique_ptr<Browser> Browser::Create(
    ios::ChromeBrowserState* browser_state) {
  return std::make_unique<BrowserImpl>(browser_state);
}
