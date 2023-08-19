// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_response_bridge.h"

StartSuggestServiceResponseBridge::StartSuggestServiceResponseBridge(
    id<StartSuggestServiceResponseDelegating> delegate)
    : delegate_(delegate) {}

StartSuggestServiceResponseBridge::~StartSuggestServiceResponseBridge() {}

void StartSuggestServiceResponseBridge::OnSuggestionsReceived(
    std::vector<QuerySuggestion> suggestions) {
  const SEL selector = @selector(suggestionsReceived:);
  if (![delegate_ respondsToSelector:selector])
    return;

  [delegate_ suggestionsReceived:suggestions];
}

base::WeakPtr<StartSuggestServiceResponseBridge>
StartSuggestServiceResponseBridge::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
