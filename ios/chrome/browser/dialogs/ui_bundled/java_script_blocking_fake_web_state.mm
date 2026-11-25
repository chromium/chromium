// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dialogs/ui_bundled/java_script_blocking_fake_web_state.h"

#import <memory>
#import <utility>

#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "url/gurl.h"

JavaScriptBlockingFakeWebState::JavaScriptBlockingFakeWebState()
    : web::FakeWebState() {
  auto manager = std::make_unique<web::FakeNavigationManager>();
  manager->AddItem(GURL(), ui::PAGE_TRANSITION_TYPED);
  manager->SetLastCommittedItem(
      manager->GetItemAtIndex(manager->GetLastCommittedItemIndex()));
  manager_ = manager.get();
  SetNavigationManager(std::move(manager));
}

JavaScriptBlockingFakeWebState::~JavaScriptBlockingFakeWebState() = default;

void JavaScriptBlockingFakeWebState::SimulateNavigationStarted(
    bool renderer_initiated,
    bool same_document,
    ui::PageTransition transition,
    bool change_last_committed_item) {
  if (change_last_committed_item) {
    // Store simulated items in `FakeNavigationManager` so previous items
    // outlive `DidStartNavigation()` and `WebStateDestroyed()`, matching
    // `NavigationManagerImpl`.
    manager_->AddItem(GURL(), transition);
    manager_->SetLastCommittedItem(
        manager_->GetItemAtIndex(manager_->GetLastCommittedItemIndex()));
  }
  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(renderer_initiated);
  context.SetIsSameDocument(same_document);
  OnNavigationStarted(&context);
}
