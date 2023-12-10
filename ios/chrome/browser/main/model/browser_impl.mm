// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_impl.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/main/model/browser_agent_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

BrowserImpl::BrowserImpl(ChromeBrowserState* browser_state,
                         SceneState* scene_state,
                         BrowserImpl* active_browser,
                         InsertionPolicy insertion_policy,
                         ActivationPolicy activation_policy)
    : BrowserWebStateListDelegate(insertion_policy, activation_policy),
      browser_state_(browser_state),
      web_state_list_(this),
      scene_state_(scene_state),
      command_dispatcher_([[CommandDispatcher alloc] init]),
      active_browser_(active_browser ?: this) {
  DCHECK(browser_state_);
  DCHECK(active_browser_);
  DCHECK(command_dispatcher_);

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
  return &web_state_list_;
}

CommandDispatcher* BrowserImpl::GetCommandDispatcher() {
  return command_dispatcher_;
}

SceneState* BrowserImpl::GetSceneState() {
  return scene_state_;
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
  inactive_browser_ = std::make_unique<BrowserImpl>(
      browser_state_, scene_state_, /*active_browser=*/this,
      BrowserImpl::InsertionPolicy::kAttachTabHelpers,
      BrowserImpl::ActivationPolicy::kDoNothing);
  return inactive_browser_.get();
}

void BrowserImpl::DestroyInactiveBrowser() {
  CHECK(!IsInactive())
      << "This browser is the inactive one. Call this on the active one.";
  inactive_browser_.reset();
}

// static
std::unique_ptr<Browser> Browser::Create(ChromeBrowserState* browser_state,
                                         SceneState* scene_state) {
  return std::make_unique<BrowserImpl>(
      browser_state, scene_state, /*active_browser=*/nullptr,
      BrowserImpl::InsertionPolicy::kAttachTabHelpers,
      BrowserImpl::ActivationPolicy::kForceRealization);
}

// static
std::unique_ptr<Browser> Browser::CreateTemporary(
    ChromeBrowserState* browser_state) {
  return std::make_unique<BrowserImpl>(
      browser_state, /*scene_state=*/nil, /*active_browser=*/nullptr,
      BrowserImpl::InsertionPolicy::kDoNothing,
      BrowserImpl::ActivationPolicy::kDoNothing);
}
