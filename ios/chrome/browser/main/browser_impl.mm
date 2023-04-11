// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_agent_util.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserImpl::BrowserImpl(ChromeBrowserState* browser_state,
                         BrowserImpl* active_browser)
    : browser_state_(browser_state),
      command_dispatcher_([[CommandDispatcher alloc] init]),
      active_browser_(active_browser ?: this) {
  DCHECK(browser_state_);
  DCHECK(active_browser_);

  web_state_list_delegate_ = std::make_unique<BrowserWebStateListDelegate>();
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());

  AttachBrowserAgents(this);
}

BrowserImpl::~BrowserImpl() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

ChromeBrowserState* BrowserImpl::GetBrowserState() {
  return browser_state_;
}

WebStateList* BrowserImpl::GetWebStateList() {
  return web_state_list_.get();
}

CommandDispatcher* BrowserImpl::GetCommandDispatcher() {
  return command_dispatcher_;
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<Browser> BrowserImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BrowserImpl::IsInactive() const {
  return active_browser_ != this;
}

Browser* BrowserImpl::GetActiveBrowser() {
  return active_browser_;
}

Browser* BrowserImpl::GetInactiveBrowser() {
  return IsInactive() ? this : inactive_browser_.get();
}

Browser* BrowserImpl::CreateInactiveBrowser() {
  CHECK(!IsInactive()) << "This browser already is the inactive one.";
  CHECK(!inactive_browser_.get())
      << "This browser already links to its inactive counterpart.";
  inactive_browser_ = std::make_unique<BrowserImpl>(browser_state_, this);
  return inactive_browser_.get();
}

void BrowserImpl::DestroyInactiveBrowser() {
  CHECK(!IsInactive())
      << "This browser is the inactive one. Call this on the active one.";
  inactive_browser_.reset();
}

// static
std::unique_ptr<Browser> Browser::Create(ChromeBrowserState* browser_state) {
  return std::make_unique<BrowserImpl>(browser_state);
}
