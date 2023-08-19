// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/remote_suggestions_service_observer_bridge.h"

RemoteSuggestionsServiceObserverBridge::RemoteSuggestionsServiceObserverBridge(
    id<RemoteSuggestionsServiceObserver> observer)
    : observer_(observer) {}

void RemoteSuggestionsServiceObserverBridge::OnSuggestRequestCreated(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  // TODO: add remote suggestion service arg
  [observer_ remoteSuggestionsService:nil
                      startingRequest:request
                     uniqueIdentifier:request_id];
}

void RemoteSuggestionsServiceObserverBridge::OnSuggestRequestStarted(
    const base::UnguessableToken& request_id,
    network::SimpleURLLoader* loader,
    const std::string& request_body) {
  // TODO: notify the observer. For the existing applications on iOS this is
  //  called immediately after `OnSuggestRequestCreated`. But it is possible for
  //  this to be called asynchronously in the future.
}

void RemoteSuggestionsServiceObserverBridge::OnSuggestRequestCompleted(
    const base::UnguessableToken& request_id,
    const int response_code,
    const std::unique_ptr<std::string>& response_body) {
  NSString* response_string = nil;
  if (response_code == 200 && response_body) {
    response_string = base::SysUTF8ToNSString(*response_body.get());
  }
  // TODO: add remote suggestion service arg
  [observer_ remoteSuggestionsService:nil
       completedRequestWithIdentifier:request_id
                     receivedResponse:response_string];
}
