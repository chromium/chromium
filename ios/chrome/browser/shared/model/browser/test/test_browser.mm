// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

TestBrowser::TestBrowser(
    ProfileIOS* profile,
    SceneState* scene_state,
    std::unique_ptr<WebStateListDelegate> web_state_list_delegate,
    Type type)
    : type_(type),
      profile_(profile),
      scene_state_(scene_state),
      web_state_list_delegate_(std::move(web_state_list_delegate)),
      command_dispatcher_([[CommandDispatcher alloc] init]) {
  DCHECK(profile_);
  DCHECK(web_state_list_delegate_);
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());
}

TestBrowser::TestBrowser(ProfileIOS* profile, SceneState* scene_state)
    : TestBrowser(
          profile,
          scene_state,
          std::make_unique<FakeWebStateListDelegate>(),
          profile->IsOffTheRecord() ? Type::kIncognito : Type::kRegular) {}

TestBrowser::TestBrowser(
    ProfileIOS* profile,
    std::unique_ptr<WebStateListDelegate> web_state_list_delegate)
    : TestBrowser(
          profile,
          nil,
          std::move(web_state_list_delegate),
          profile->IsOffTheRecord() ? Type::kIncognito : Type::kRegular) {}

TestBrowser::TestBrowser(ProfileIOS* profile)
    : TestBrowser(
          profile,
          nil,
          std::make_unique<FakeWebStateListDelegate>(),
          profile->IsOffTheRecord() ? Type::kIncognito : Type::kRegular) {}

TestBrowser::~TestBrowser() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

#pragma mark - Browser

Browser::Type TestBrowser::type() const {
  return type_;
}

ProfileIOS* TestBrowser::GetProfile() {
  return profile_;
}

WebStateList* TestBrowser::GetWebStateList() {
  return web_state_list_.get();
}

CommandDispatcher* TestBrowser::GetCommandDispatcher() {
  return command_dispatcher_;
}

SceneState* TestBrowser::GetSceneState() {
  return scene_state_;
}

void TestBrowser::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void TestBrowser::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<Browser> TestBrowser::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool TestBrowser::IsInactive() const {
  return type_ == Type::kInactive;
}

Browser* TestBrowser::GetActiveBrowser() {
  return this;
}

Browser* TestBrowser::GetInactiveBrowser() {
  return nullptr;
}

Browser* TestBrowser::CreateInactiveBrowser() {
  CHECK_EQ(type_, Type::kRegular);
  inactive_browser_ = std::make_unique<TestBrowser>(
      profile_, scene_state_, std::make_unique<FakeWebStateListDelegate>(),
      Type::kInactive);
  SnapshotBrowserAgent::CreateForBrowser(inactive_browser_.get());
  SnapshotBrowserAgent::FromBrowser(inactive_browser_.get())
      ->SetSessionID("some_id");
  return inactive_browser_.get();
}

void TestBrowser::DestroyInactiveBrowser() {
  NOTREACHED();
}
