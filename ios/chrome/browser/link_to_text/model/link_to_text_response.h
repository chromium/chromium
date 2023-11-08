// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_RESPONSE_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_RESPONSE_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/time/time.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/link_to_text/model/link_generation_outcome.h"
#import "url/gurl.h"

namespace base {
class Value;
}

@class LinkToTextPayload;

namespace web {
class WebState;
}

// Response object for calls to generate a link-to-text deep-link, could contain
// either a payload or an error.
@interface LinkToTextResponse : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Parses a serialized response stored in `value` into a LinkToTextResponse
// instance, along with the associated `webState` and `latency` for generating
// the response.
+ (instancetype)linkToTextResponseWithValue:(const base::Value*)value
                                   webState:(web::WebState*)webState
                                    latency:(base::TimeDelta)latency;

// Response payload. Nil when an error occurred.
@property(nonatomic, readonly) LinkToTextPayload* payload;

// Error which occurred when trying to generate a link. Empty when `payload`
// has a value.
@property(nonatomic, readonly)
    std::optional<shared_highlighting::LinkGenerationError>
        error;

// Source ID for the associated WebState.
@property(nonatomic, readonly) ukm::SourceId sourceID;

// Time taken to generate a link.
@property(nonatomic, readonly) base::TimeDelta latency;

@end

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_RESPONSE_H_
