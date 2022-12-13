// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/java_script_blocking_fake_web_state.h"

#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

JavaScriptBlockingFakeWebState::JavaScriptBlockingFakeWebState()
    : web::FakeWebState() {
  last_committed_item_ = web::NavigationItem::Create();
  auto manager = std::make_unique<web::FakeNavigationManager>();
  manager->SetLastCommittedItem(last_committed_item_.get());
  manager_ = manager.get();
  SetNavigationManager(std::move(manager));
}

JavaScriptBlockingFakeWebState::~JavaScriptBlockingFakeWebState() {}

void JavaScriptBlockingFakeWebState::SimulateNavigationStarted(
    bool renderer_initiated,
    bool same_document,
    ui::PageTransition transition,
    bool change_last_committed_item) {
  if (change_last_committed_item) {
    last_committed_item_ = web::NavigationItem::Create();
    manager_->SetLastCommittedItem(last_committed_item_.get());
  }
  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(renderer_initiated);
  context.SetIsSameDocument(same_document);
  OnNavigationStarted(&context);
}
