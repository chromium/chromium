// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_ACCEPTANCE_DATA_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_ACCEPTANCE_DATA_H_

#import <Foundation/Foundation.h>

// Data object used to store information about the acceptance of a promo.
@interface InstallAttributionAcceptanceData : NSObject <NSSecureCoding>

// The placement ID of the promo that was accepted.
@property(nonatomic, copy) NSNumber* placementID;

// The timestamp of when the promo was accepted.
@property(nonatomic, copy) NSDate* timestamp;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_INSTALL_ATTRIBUTION_ACCEPTANCE_DATA_H_
