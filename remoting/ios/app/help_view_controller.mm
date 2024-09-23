// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/help_view_controller.h"

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/web_view_controller.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(nicholss): These urls should come from a global config.
static NSString* const kHelpCenterUrl =
    @"https://support.google.com/chrome/answer/1649523?co=GENIE.Platform%3DiOS";

static NSString* const kCreditsUrlString =
    [[NSBundle mainBundle] URLForResource:@"credits" withExtension:@"html"]
        .absoluteString;

@implementation HelpViewController

- (instancetype)init {
  if ((self = [super initWithURL:[NSURL URLWithString:kHelpCenterUrl]])) {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_ACTIONBAR_HELP_TITLE);
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CREDITS)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(onTapCredits:)];
  }
  return self;
}

#pragma mark - Private

- (void)onTapCredits:(id)button {
  WebViewController* creditsVC = [[WebViewController alloc]
      initWithUrl:kCreditsUrlString
            title:l10n_util::GetNSString(IDS_CREDITS)];
  [self.navigationController pushViewController:creditsVC animated:YES];
}

@end
