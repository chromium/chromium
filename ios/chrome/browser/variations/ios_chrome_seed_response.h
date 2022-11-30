// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_SEED_RESPONSE_H_
#define IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_SEED_RESPONSE_H_

#import <Foundation/Foundation.h>

// iOS implementation of components/variations/seed_response.h.
@interface IOSChromeSeedResponse : NSObject

@property(nonatomic, strong, readonly) NSString* signature;
@property(nonatomic, strong, readonly) NSString* country;
@property(nonatomic, strong, readonly) NSDate* time;
@property(nonatomic, assign, readonly) BOOL compressed;
@property(nonatomic, strong, readonly) NSData* data;

- (instancetype)initWithSignature:(NSString*)signature
                          country:(NSString*)country
                             time:(NSDate*)time
                             data:(NSData*)data
                       compressed:(BOOL)compressed;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_IOS_CHROME_SEED_RESPONSE_H_
