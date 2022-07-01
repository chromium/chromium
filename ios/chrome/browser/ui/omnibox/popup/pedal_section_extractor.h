// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_PEDAL_SECTION_EXTRACTOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_PEDAL_SECTION_EXTRACTOR_H_

#import "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"

@protocol PopupMatchPreviewDelegate;

// Extractor can be inserted in between the OmniboxPopupMediator and its
// consumer. It acts as a consumer and a consumer delegate, and is intended to
// intercept any calls in these protocols.
// It extracts any pedals being pushed to it, wraps them to look like
// suggestions, and passes it down to `dataSink`.
// It also intercepts the return key (by proxying OmniboxReturnDelegate) to
// allow "return" key to execute the pedal whenever it selected; if a non-pedal
// is highlighted, it forwards the call to its accept delegate.
// When a pedal is highlighted, it also tells the match preview delegate
// to display the corresponding image and text.
@interface PedalSectionExtractor : NSObject <AutocompleteResultConsumer,
                                             AutocompleteResultConsumerDelegate,
                                             OmniboxReturnDelegate>

// The sink to forward AutocompleteResultConsumer calls.
@property(nonatomic, weak) id<AutocompleteResultConsumer> dataSink;
// The delegate to forward AutocompleteResultConsumerDelegate calls to.
@property(nonatomic, weak) id<AutocompleteResultConsumerDelegate> delegate;
// The delegate that receives text/image when a pedal is highlighted.
@property(nonatomic, weak) id<PopupMatchPreviewDelegate> matchPreviewDelegate;
@property(nonatomic, weak) id<OmniboxReturnDelegate> acceptDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_PEDAL_SECTION_EXTRACTOR_H_
