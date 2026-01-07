// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/time/time.h"
#import "ui/base/window_open_disposition.h"

class AutocompleteController;
struct AutocompleteMatch;
class GURL;
@class OmniboxAutocompleteController;
class OmniboxClient;
struct OmniboxPopupSelection;
struct OmniboxTextModel;

@interface OmniboxMetricsRecorder : NSObject

/// OmniboxAutocompleteController used to retrieve the popup state.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

/// Creates an instance with the dependency used for state retrieval.
- (instancetype)initWithClient:(OmniboxClient*)omniboxClient
                     textModel:(const OmniboxTextModel*)omniboxTextModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes C++ references.
- (void)disconnect;

/// Sets the autocompleteController.
- (void)setAutocompleteController:
    (AutocompleteController*)autocompleteController;

/// Records metrics for opening an omnibox suggestion or navigating to an URL.
- (void)recordOpenMatch:(AutocompleteMatch)match
           destinationURL:(GURL)destinationURL
                inputText:(const std::u16string&)inputText
           popupSelection:(OmniboxPopupSelection)selection
    windowOpenDisposition:(WindowOpenDisposition)disposition
                 isAction:(BOOL)isAction
             isPastedText:(BOOL)isPastedText;

/// Returns the elapsed time since the user first modified the omnibox.
/// Returns kDefaultTimeDelta if the input is zero suggest or pasted text.
- (base::TimeDelta)elapsedTimeSinceUserFirstModifiedOmniboxWithPastedText:
    (BOOL)isPastedText;

// Records the number of lines in the omnibox text view.
- (void)setNumberOfLines:(NSInteger)numberOfLines;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_METRICS_RECORDER_H_
