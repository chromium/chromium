// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/remote_suggestions_service_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

RemoteSuggestionsServiceObserverBridge::RemoteSuggestionsServiceObserverBridge(
    id<RemoteSuggestionsServiceObserver> observer)
    : observer_(observer) {}

void RemoteSuggestionsServiceObserverBridge::OnSuggestRequestStarting(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  // TODO: add remote suggestion service arg
  [observer_ remoteSuggestionsService:nil
                      startingRequest:request
                     uniqueIdentifier:request_id];
}

void RemoteSuggestionsServiceObserverBridge::OnSuggestRequestCompleted(
    const base::UnguessableToken& request_id,
    const bool response_received,
    const std::unique_ptr<std::string>& response_body) {
  NSString* response_string = nil;
  if (response_received && response_body) {
    response_string = base::SysUTF8ToNSString(*response_body.get());
  }
  // TODO: add remote suggestion service arg
  [observer_ remoteSuggestionsService:nil
       completedRequestWithIdentifier:request_id
                     receivedResponse:response_string];
}
