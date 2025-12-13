// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_edit_menu_builder.h"

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"
#import "ios/web/public/web_state.h"

@implementation DataControlsEditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  data_controls::DataControlsTabHelper* tab_helper =
      data_controls::DataControlsTabHelper::GetOrCreateForWebState(webState);
  if (tab_helper && !tab_helper->ShouldAllowShare()) {
    [builder removeMenuForIdentifier:UIMenuShare];
  }
}

@end
