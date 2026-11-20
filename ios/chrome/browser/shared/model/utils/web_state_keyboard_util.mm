// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/web_state_keyboard_util.h"

#import <UIKit/UIKit.h>

#import "ios/web/public/web_state.h"

void DismissKeyboardForWebState(web::WebState* web_state) {
  // An unrealized `WebState` has no view and cannot be showing a keyboard.
  if (!web_state || !web_state->IsRealized()) {
    return;
  }
  [web_state->GetView() endEditing:NO];
}
