// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_availability.h"

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool MaybeShowAIMPrototype(Browser* browser,
                           AIMPrototypeEntrypoint entrypoint,
                           NSString* query) {
  if (!base::FeatureList::IsEnabled(kAIMPrototype)) {
    return false;
  }

  std::string param =
      base::GetFieldTrialParamValueByFeature(kAIMPrototype, kAIMPrototypeParam);
  BOOL showPrototype = entrypoint == AIMPrototypeEntrypoint::kNTPAIMButton ||
                       param == kAIMPrototypeParamAllOmniboxEntrypoints;

  if (showPrototype) {
    id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [commands showAIMPrototypeFromEntrypoint:entrypoint withQuery:query];
  }

  return showPrototype;
}
