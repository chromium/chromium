// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_GMO_SKO_ACCEPTANCE_DATA_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_GMO_SKO_ACCEPTANCE_DATA_H_

#import <Foundation/Foundation.h>

// This class is a data container for information about a promo outside
// Chromium. It contains the placement ID and the timestamp of the acceptance.
// It conforms to NSSecureCoding to be safely archived/unarchived. The class is
// copied from its counterpart outside the Chromium codebase.
@interface GMOSKOAcceptanceData : NSObject <NSSecureCoding>

// An ID representing the campaign of the accepted promo.
@property(nonatomic, copy, readonly) NSNumber* placementID;

// The timestamp at which the promo was accepted.
@property(nonatomic, copy, readonly) NSDate* timestamp;

// A designated initializer to ensure the object is always created with valid
// data.
- (instancetype)initWithPlacementID:(NSNumber*)placementID
                          timestamp:(NSDate*)timestamp
    NS_DESIGNATED_INITIALIZER;

// Disallow use of init.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_INSTALL_ATTRIBUTION_GMO_SKO_ACCEPTANCE_DATA_H_
