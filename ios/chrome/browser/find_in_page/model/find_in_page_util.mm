// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/find_in_page_util.h"

#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

bool IsFindNavigatorVisibleInTab(web::WebState* tab) {
  web::WebState* target_web_state = tab;
  // If the tab has Reader mode active, use the Reader Mode web state.
  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(tab);
  if (reader_mode_tab_helper) {
    web::WebState* reader_mode_web_state =
        reader_mode_tab_helper->GetReaderModeWebState();
    if (reader_mode_web_state) {
      target_web_state = reader_mode_web_state;
    }
  }
  return FindTabHelper::FromWebState(target_web_state)->IsFindUIActive();
}
