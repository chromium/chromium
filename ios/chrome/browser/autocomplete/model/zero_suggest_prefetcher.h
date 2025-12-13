// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_PREFETCHER_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_PREFETCHER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteController;
namespace web {
class WebState;
}  // namespace web
class WebStateList;

@interface ZeroSuggestPrefetcher : NSObject

/// Initialize with web state list observation.
- (instancetype)
    initWithAutocompleteController:
        (AutocompleteController*)autocompleteController
                      webStateList:(WebStateList*)webStateList
            classificationCallback:
                (base::RepeatingCallback<
                    metrics::OmniboxEventProto::PageClassification()>)
                    classificationCallback
                disconnectCallback:
                    (base::OnceCallback<void(WebStateList*)>)disconnectCallback;

/// Initialize with web state observation.
- (instancetype)
    initWithAutocompleteController:
        (AutocompleteController*)autocompleteController
                          webState:(web::WebState*)webState
            classificationCallback:
                (base::RepeatingCallback<
                    metrics::OmniboxEventProto::PageClassification()>)
                    classificationCallback
                disconnectCallback:(base::OnceCallback<void(web::WebState*)>)
                                       disconnectCallback;

/// Disconnect all observations and references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_PREFETCHER_H_
