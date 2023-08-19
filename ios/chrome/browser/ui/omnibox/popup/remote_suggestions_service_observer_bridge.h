// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_

#include "components/omnibox/browser/remote_suggestions_service.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"

@protocol RemoteSuggestionsServiceObserver
- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
                 startingRequest:(const network::ResourceRequest*)request
                uniqueIdentifier:
                    (const base::UnguessableToken&)requestIdentifier;

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    completedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                  receivedResponse:(NSString*)response;
@end

class RemoteSuggestionsServiceObserverBridge
    : public RemoteSuggestionsService::Observer {
 public:
  RemoteSuggestionsServiceObserverBridge(
      id<RemoteSuggestionsServiceObserver> observer);

  RemoteSuggestionsServiceObserverBridge(
      const RemoteSuggestionsServiceObserverBridge&) = delete;
  RemoteSuggestionsServiceObserverBridge& operator=(
      const RemoteSuggestionsServiceObserverBridge&) = delete;

  void OnSuggestRequestCreated(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override;

  void OnSuggestRequestStarted(const base::UnguessableToken& request_id,
                               network::SimpleURLLoader* loader,
                               const std::string& request_body) override;

  void OnSuggestRequestCompleted(
      const base::UnguessableToken& request_id,
      const int response_code,
      const std::unique_ptr<std::string>& response_body) override;

 private:
  __weak id<RemoteSuggestionsServiceObserver> observer_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_REMOTE_SUGGESTIONS_SERVICE_OBSERVER_BRIDGE_H_
