// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_coordinator.h"

#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_coordinator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"

@implementation ComposeboxInttestCoordinator {
  ComposeboxCoordinator* _composeboxCoordinator;
}

- (void)start {
  // Initialize dependencies.
  AutocompleteBrowserAgent::CreateForBrowser(self.browser);

  // Start coordinator.
  _composeboxCoordinator = [[ComposeboxCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      entrypoint:ComposeboxEntrypoint::kOther
                           query:nil
         composeboxAnimationBase:nil];
  [_composeboxCoordinator start];
}

- (void)stop {
  // Stop coordinator.
  [_composeboxCoordinator stop];
  _composeboxCoordinator = nil;

  // Cleanup dependencies.
  AutocompleteBrowserAgent* autocompleteBrowserAgent =
      AutocompleteBrowserAgent::FromBrowser(self.browser);
  if (autocompleteBrowserAgent) {
    // Cleanup test autocomplete that might have been added.
    autocompleteBrowserAgent->RemoveServices();
  }
}

@end
