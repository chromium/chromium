// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/util/chrome_button.h"

namespace {

// Creates the default button stack configuration using the tertiary style so
// both options are equally weighted.
ButtonStackConfiguration* SetUpButtonConfiguration() {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryButtonStyle = ChromeButtonStyleTertiary;
  configuration.secondaryButtonStyle = ChromeButtonStyleTertiary;
  return configuration;
}

}  // namespace

@implementation RemoteActorCredentialBottomSheetViewController

- (instancetype)init {
  return [super initWithConfiguration:SetUpButtonConfiguration()];
}

#pragma mark - TableViewBottomSheetViewController

- (NSUInteger)rowCount {
  // TODO(crbug.com/579012416): Update `rowCount` to return the number of
  // suggestions to display.
  return 0;
}

@end
