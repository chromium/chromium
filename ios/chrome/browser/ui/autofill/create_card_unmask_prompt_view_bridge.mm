// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/create_card_unmask_prompt_view_bridge.h"

#import "base/feature_list.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/autofill/legacy_card_unmask_prompt_view_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

CardUnmaskPromptView* CreateCardUnmaskPromptViewBridge(
    CardUnmaskPromptController* unmask_controller,
    UIViewController* base_view_controller) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableNewCardUnmaskPromptView)) {
    return new CardUnmaskPromptViewBridge(unmask_controller,
                                          base_view_controller);
  }

  return new LegacyCardUnmaskPromptViewBridge(unmask_controller,
                                              base_view_controller);
}

}  // namespace autofill
