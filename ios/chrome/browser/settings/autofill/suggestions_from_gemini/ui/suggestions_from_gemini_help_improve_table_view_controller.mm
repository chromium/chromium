// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation SuggestionsFromGeminiHelpImproveTableViewController {
  BOOL _settingsAreDismissed;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/539811785): Implement navigation metrics.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/539811785): Implement navigation metrics.
}

- (void)settingsWillBeDismissed {
  _settingsAreDismissed = YES;
}

@end
