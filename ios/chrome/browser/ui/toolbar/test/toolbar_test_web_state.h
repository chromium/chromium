// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_WEB_STATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_WEB_STATE_H_

#import "ios/web/public/test/fakes/fake_web_state.h"

class ToolbarTestWebState : public web::FakeWebState {
 public:
  ToolbarTestWebState();

  double GetLoadingProgress() const override;
  void set_loading_progress(double loading_progress);

 private:
  double loading_progress_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarTestWebState);
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_WEB_STATE_H_
