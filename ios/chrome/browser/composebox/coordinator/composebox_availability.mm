// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_availability.h"

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool MaybeShowComposebox(Browser* browser,
                         ComposeboxEntrypoint entrypoint,
                         NSString* query) {
  if (!IsComposeboxIOSEnabled()) {
    return false;
  }
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [commands showComposeboxFromEntrypoint:entrypoint withQuery:query];
  return true;
}
