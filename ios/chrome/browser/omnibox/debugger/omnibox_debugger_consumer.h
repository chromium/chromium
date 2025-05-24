// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_CONSUMER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "components/omnibox/browser/remote_suggestions_service.h"

@protocol OmniboxRemoteSuggestionEvent;
@protocol OmniboxEvent;

/// An abstract consumer of omnibox popup debug info.
@protocol OmniboxDebuggerConsumer <NSObject>

/// Gives the consumer a new variation IDs string to display.
- (void)setVariationIDString:(NSString*)string;

/// Removes all objects from the debug info.
- (void)removeAllObjects;

/// Informs the consumer about a new omnibox event.
- (void)registerNewOmniboxEvent:(id<OmniboxEvent>)event;

/// Updates the status of a remote suggestion event using the provided request
/// body.
- (void)updateRemoteSuggestionEventWithRequestIdentifier:
            (const base::UnguessableToken&)requestIdentifier
                                             requestBody:(NSString*)requestBody;

/// Updates the status of a remote suggestion event using the provided response
/// details.
- (void)updateRemoteSuggestionEventWithRequestIdentifier:
            (const base::UnguessableToken&)requestIdentifier
                                            responseBody:(NSString*)responseBody
                                            responseCode:(NSInteger)code;
@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_CONSUMER_H_
