// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_RESPONSE_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_RESPONSE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "components/search/start_suggest_service.h"

@protocol StartSuggestServiceResponseDelegating <NSObject>
- (void)suggestionsReceived:(std::vector<QuerySuggestion>)suggestions;
@end

// Adapter to receive StartSuggestService callback responses through
// StartSuggestServiceResponseDelegating.
class StartSuggestServiceResponseBridge {
 public:
  explicit StartSuggestServiceResponseBridge(
      id<StartSuggestServiceResponseDelegating> delegate);

  StartSuggestServiceResponseBridge(const StartSuggestServiceResponseBridge&) =
      delete;
  StartSuggestServiceResponseBridge& operator=(
      const StartSuggestServiceResponseBridge&) = delete;

  ~StartSuggestServiceResponseBridge();

  void OnSuggestionsReceived(std::vector<QuerySuggestion> suggestions);

  // Return a weak pointer to the current object.
  base::WeakPtr<StartSuggestServiceResponseBridge> AsWeakPtr();

 private:
  // StartSuggestServiceResponseDelegating which receives forwarded calls.
  __weak id<StartSuggestServiceResponseDelegating> delegate_ = nil;

  base::WeakPtrFactory<StartSuggestServiceResponseBridge> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_RESPONSE_BRIDGE_H_
