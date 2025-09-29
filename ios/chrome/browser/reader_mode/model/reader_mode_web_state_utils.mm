// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"

#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/web/public/web_state.h"

bool IsReaderModeActiveInWebState(web::WebState* web_state) {
  if (!IsReaderModeAvailable() || !web_state) {
    return false;
  }
  ReaderModeTabHelper* tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (tab_helper) {
    return tab_helper->IsActive();
  }

  return false;
}
