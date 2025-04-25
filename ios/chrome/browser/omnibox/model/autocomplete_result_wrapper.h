// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteResultWrapperDelegate;
@protocol AutocompleteSuggestion;
@protocol AutocompleteSuggestionGroup;
@class AutocompleteMatchFormatter;
class AutocompleteResult;
class OmniboxClient;
@class OmniboxPedalAnnotator;
class TemplateURLService;

// The autocomplete match wrapper. This class is responsible for wrapping
// AutocompleteResult.
@interface AutocompleteResultWrapper : NSObject

/// Initializes the wrapper with the given omnibox client.
- (instancetype)initWithOmniboxClient:(OmniboxClient*)omniboxClient
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// The autocomplete match wrapper delegate.
@property(nonatomic, weak) id<AutocompleteResultWrapperDelegate> delegate;

/// The annotator to create pedals for ths mediator.
@property(nonatomic, strong) OmniboxPedalAnnotator* pedalAnnotator;

/// Whether or not browser is in incognito mode.
@property(nonatomic, assign) BOOL incognito;

/// TemplateURLService to observe default search engine change.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// Whether the omnibox has a thumbnail.
@property(nonatomic, assign) BOOL hasThumbnail;

/// Disconnects the wrapper.
- (void)disconnect;

/// Organizes the raw autocomplete result into structured groups of suggestions.
- (NSArray<id<AutocompleteSuggestionGroup>>*)wrapAutocompleteResultInGroups:
    (const AutocompleteResult&)autocompleteResult;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_H_
