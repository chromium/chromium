// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/coordinator/default_browser_mediator.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_config.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@interface DefaultBrowserMediator () <DefaultBrowserCommands>

@end

@implementation DefaultBrowserMediator {
}

- (instancetype)init {
  if ((self = [super init])) {
    self.config = [[DefaultBrowserConfig alloc] init];
    self.config.commandHandler = self;
  }
  return self;
}

- (void)disconnect {
  self.config = nil;
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeDefaultBrowserPromoModuleWithCompletion:completion];
}

- (void)didTapDefaultBrowserPromo {
  [self.presentationAudience didTapDefaultBrowserPromo];
}

@end
