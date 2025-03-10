// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteMatchWrapperDelegate;
@protocol AutocompleteSuggestion;
@protocol AutocompleteSuggestionGroup;
@class AutocompleteMatchFormatter;
class AutocompleteResult;
struct AutocompleteMatch;
@class OmniboxPedalAnnotator;
class TemplateURLService;

// The autocomplete match wrapper. This class is responsible for wrapping
// AutocompleteMatch and AutocompleteResult.
@interface AutocompleteMatchWrapper : NSObject

/// The autocomplete match wrapper delegate.
@property(nonatomic, weak) id<AutocompleteMatchWrapperDelegate> delegate;

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

/// Wraps the autocomplete results from the given AutocompleteResult object into
/// an array of AutocompleteSuggestion objects.
- (NSMutableArray<AutocompleteMatchFormatter*>*)wrapMatchesFromResult:
    (const AutocompleteResult&)autocompleteResult;

/// Take a list of suggestions and break it into groups determined by sectionId
/// field. Use `headerMap` to extract group names.
- (NSArray<id<AutocompleteSuggestionGroup>>*)
            groupSuggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
    usingACResultAsHeaderMap:(const AutocompleteResult&)headerMap;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_MATCH_WRAPPER_H_
