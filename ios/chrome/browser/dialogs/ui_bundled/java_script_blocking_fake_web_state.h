// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_JAVA_SCRIPT_BLOCKING_FAKE_WEB_STATE_H_
#define IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_JAVA_SCRIPT_BLOCKING_FAKE_WEB_STATE_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

namespace web {
class NavigationItem;
class FakeNavigationManager;
}  // namespace web

// FakeWebState subclass that allows simulating page loads.
class JavaScriptBlockingFakeWebState : public web::FakeWebState {
 public:
  JavaScriptBlockingFakeWebState();
  ~JavaScriptBlockingFakeWebState() override;

  // Simulates a navigation by sending a WebStateObserver callback.
  void SimulateNavigationStarted(bool renderer_initiated,
                                 bool same_document,
                                 ui::PageTransition transition,
                                 bool change_last_committed_item);

 private:
  raw_ptr<web::FakeNavigationManager> manager_ = nullptr;
  std::unique_ptr<web::NavigationItem> last_committed_item_;
};

#endif  // IOS_CHROME_BROWSER_DIALOGS_UI_BUNDLED_JAVA_SCRIPT_BLOCKING_FAKE_WEB_STATE_H_
