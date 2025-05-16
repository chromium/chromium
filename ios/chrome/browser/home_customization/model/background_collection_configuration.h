// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_COLLECTION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_COLLECTION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class BackgroundCustomizationConfiguration;

// A tuple for grouping a collection name with its associated array of
// `BackgroundCustomizationConfiguration`.
@interface BackgroundCollectionConfiguration : NSObject

// The name of the background collection.
@property(nonatomic, copy) NSString* collectionName;

// The background customization configurations associated with this collection.
@property(nonatomic, strong)
    NSArray<BackgroundCustomizationConfiguration*>* configurations;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_COLLECTION_CONFIGURATION_H_
