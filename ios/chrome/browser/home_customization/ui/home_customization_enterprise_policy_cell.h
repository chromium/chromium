// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ENTERPRISE_POLICY_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ENTERPRISE_POLICY_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"

// A cell to display the "Managed by your organization" message.
@interface HomeCustomizationEnterprisePolicyCell : UICollectionViewCell

- (void)configureCellWithMutator:(id<HomeCustomizationMutator>)mutator;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ENTERPRISE_POLICY_CELL_H_
