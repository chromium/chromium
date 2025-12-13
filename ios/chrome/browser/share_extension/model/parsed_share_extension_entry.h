// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_PARSED_SHARE_EXTENSION_ENTRY_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_PARSED_SHARE_EXTENSION_ENTRY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/app_group/app_group_constants.h"

class GaiaId;

// An interface that represents a parsed share extension entry.
@interface ParsedShareExtensionEntry : NSObject

@property(nonatomic, assign) BOOL cancelled;
@property(nonatomic, strong) NSURL* url;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) NSDate* date;
@property(nonatomic, assign) app_group::ShareExtensionItemType type;
@property(nonatomic, copy) NSString* source;
@property(nonatomic, assign) GaiaId gaiaID;

// Check whether a parsed entry is valid. An entry is considered valid if it has
// a source, a date and a type, if it represents a URL, the URL should be valid
// as well.
- (BOOL)parsedEntryIsValid;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_PARSED_SHARE_EXTENSION_ENTRY_H_
