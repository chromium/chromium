// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/debugger/remote_suggestions_service_observer_bridge.h"

#import <string>

#import "base/types/optional_ref.h"

RemoteSuggestionsServiceObserverBridge::RemoteSuggestionsServiceObserverBridge(
    id<RemoteSuggestionsServiceObserver> observer,
    RemoteSuggestionsService* remote_suggestions_service)
    : observer_(observer),
      remote_suggestions_service_(remote_suggestions_service) {}

void RemoteSuggestionsServiceObserverBridge::OnRequestCreated(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest* request) {
  [observer_ remoteSuggestionsService:remote_suggestions_service_
         createdRequestWithIdentifier:request_id
                              request:request];
}

void RemoteSuggestionsServiceObserverBridge::OnRequestStarted(
    const base::UnguessableToken& request_id,
    network::SimpleURLLoader* loader,
    const std::string& request_body) {
  NSString* requestBody = base::SysUTF8ToNSString(request_body);
  [observer_ remoteSuggestionsService:remote_suggestions_service_
         startedRequestWithIdentifier:request_id
                          requestBody:requestBody
                            URLLoader:loader];
}

void RemoteSuggestionsServiceObserverBridge::OnRequestCompleted(
    const base::UnguessableToken& request_id,
    const int response_code,
    base::optional_ref<std::string> response_body) {
  NSString* responseBody = nil;
  if (response_code == 200 && response_body) {
    responseBody = base::SysUTF8ToNSString(*response_body);
  }
  [observer_ remoteSuggestionsService:remote_suggestions_service_
       completedRequestWithIdentifier:request_id
                         responseCode:response_code
                         responseBody:responseBody];
}
