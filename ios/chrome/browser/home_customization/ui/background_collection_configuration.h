// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_COLLECTION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_COLLECTION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@protocol BackgroundCustomizationConfiguration;

// A tuple for grouping a collection name with its associated dictionary of
// `BackgroundCustomizationConfiguration` and array for ordering. Anyone who
// updates data here must take care to make sure the dictionary and array stay
// in sync.
@interface BackgroundCollectionConfiguration : NSObject

// The name of the background collection, may be empty if the collection
// doesn't have a name.
@property(nonatomic, copy) NSString* collectionName;

// The background customization configurations associated with this collection.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, id<BackgroundCustomizationConfiguration>>*
        configurations;

// The order the configurations in this section should be displayed in..
@property(nonatomic, strong) NSMutableArray<NSString*>* configurationOrder;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_COLLECTION_CONFIGURATION_H_
