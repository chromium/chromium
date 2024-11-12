// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LOOKUP_GAIA_ID_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LOOKUP_GAIA_ID_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "base/types/expected.h"
#import "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#import "third_party/abseil-cpp/absl/status/status.h"

// The configuration for looking up a gaia ID from an email.
@interface ShareKitLookupGaiaIDConfiguration : NSObject

// The email to lookup the Gaia ID for. Must be non-nil.
@property(nonatomic, copy) NSString* email;

// The callback when the lookup is complete.
@property(nonatomic, strong) void (^callback)
    (const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                          absl::Status>&);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LOOKUP_GAIA_ID_CONFIGURATION_H_
