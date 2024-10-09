// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_impl.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/main/model/browser_agent_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

BrowserImpl::BrowserImpl(ProfileIOS* profile,
                         SceneState* scene_state,
                         CommandDispatcher* command_dispatcher,
                         BrowserImpl* active_browser,
                         InsertionPolicy insertion_policy,
                         ActivationPolicy activation_policy,
                         Type type)
    : BrowserWebStateListDelegate(insertion_policy, activation_policy),
      type_(type),
      profile_(profile),
      web_state_list_(this),
      scene_state_(scene_state),
      command_dispatcher_(command_dispatcher),
      active_browser_(active_browser ?: this) {
  DCHECK(profile_);
  DCHECK(active_browser_);

  CHECK((type == Type::kInactive) == (active_browser != nullptr));

  AttachBrowserAgents(this);
}

BrowserImpl::~BrowserImpl() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

Browser::Type BrowserImpl::type() const {
  return type_;
}

ProfileIOS* BrowserImpl::GetProfile() {
  return profile_;
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
  return type_ == Type::kInactive;
}

Browser* BrowserImpl::GetActiveBrowser() {
  return active_browser_;
}

Browser* BrowserImpl::GetInactiveBrowser() {
  return IsInactive() ? this : inactive_browser_.get();
}

Browser* BrowserImpl::CreateInactiveBrowser() {
  CHECK_EQ(type_, Type::kRegular);
  CHECK(!inactive_browser_.get())
      << "This browser already links to its inactive counterpart.";
  inactive_browser_ = std::make_unique<BrowserImpl>(
      profile_, scene_state_, [[CommandDispatcher alloc] init],
      /*active_browser=*/this, BrowserImpl::InsertionPolicy::kAttachTabHelpers,
      BrowserImpl::ActivationPolicy::kDoNothing, Type::kInactive);
  return inactive_browser_.get();
}

void BrowserImpl::DestroyInactiveBrowser() {
  CHECK(!IsInactive())
      << "This browser is the inactive one. Call this on the active one.";
  inactive_browser_.reset();
}

// static
std::unique_ptr<Browser> Browser::Create(ProfileIOS* profile,
                                         SceneState* scene_state) {
  const Type type =
      profile->IsOffTheRecord() ? Type::kIncognito : Type::kRegular;
  return std::make_unique<BrowserImpl>(
      profile, scene_state, [[CommandDispatcher alloc] init],
      /*active_browser=*/nullptr,
      BrowserImpl::InsertionPolicy::kAttachTabHelpers,
      BrowserImpl::ActivationPolicy::kForceRealization, type);
}

// static
std::unique_ptr<Browser> Browser::CreateTemporary(ProfileIOS* profile) {
  return std::make_unique<BrowserImpl>(
      profile, /*scene_state=*/nil, /*command_dispatcher=*/nil,
      /*active_browser=*/nullptr, BrowserImpl::InsertionPolicy::kDoNothing,
      BrowserImpl::ActivationPolicy::kDoNothing, Type::kTemporary);
}
