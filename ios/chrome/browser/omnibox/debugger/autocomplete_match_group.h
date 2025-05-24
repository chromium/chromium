// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_AUTOCOMPLETE_MATCH_GROUP_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_AUTOCOMPLETE_MATCH_GROUP_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/model/autocomplete_match_formatter.h"

/// A class that store a list of autocomplete matches with a title.
@interface AutocompleteMatchGroup : NSObject

@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) NSArray<AutocompleteMatchFormatter*>* matches;

+ (AutocompleteMatchGroup*)
    groupWithTitle:(NSString*)title
           matches:(NSArray<AutocompleteMatchFormatter*>*)matches;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_AUTOCOMPLETE_MATCH_GROUP_H_
