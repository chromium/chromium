// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_

#import <UIKit/UIKit.h>

struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteMatchFormatter;
@class OmniboxPedalAnnotator;
class TemplateURLService;

// The autocomplete match wrapper. This class is responsible for wrapping
// AutocompleteMatch and AutocompleteResult.
@interface AutocompleteMatchWrapper : NSObject

/// The annotator to create pedals for ths mediator.
@property(nonatomic, weak) OmniboxPedalAnnotator* pedalAnnotator;

/// Whether or not browser is in incognito mode.
@property(nonatomic, assign) BOOL isIncognito;

/// TemplateURLService to observe default search engine change.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// Whether the omnibox has a thumbnail.
@property(nonatomic, assign) BOOL hasThumbnail;

/// Disconnects the wrapper.
- (void)disconnect;

/// Wraps `match` with AutocompleteMatchFormatter.
- (AutocompleteMatchFormatter*)wrapMatch:(const AutocompleteMatch&)match
                              fromResult:(const AutocompleteResult&)result
                               isStarred:(BOOL)isStarred;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_
