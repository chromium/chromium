// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/debugger/remote_suggestions_service_delegate_bridge.h"

#import <optional>
#import <string>

RemoteSuggestionsServiceDelegateBridge::RemoteSuggestionsServiceDelegateBridge(
    id<RemoteSuggestionsServiceDelegate> delegate,
    RemoteSuggestionsService* remote_suggestions_service)
    : delegate_(delegate),
      remote_suggestions_service_(remote_suggestions_service) {}

RemoteSuggestionsServiceDelegateBridge::
    ~RemoteSuggestionsServiceDelegateBridge() {
  remote_suggestions_service_ = nullptr;
  delegate_ = nil;
}

base::WeakPtr<RemoteSuggestionsServiceDelegateBridge>
RemoteSuggestionsServiceDelegateBridge::AsWeakPtr() {
  return this->weak_ptr_factory_.GetWeakPtr();
}

void RemoteSuggestionsServiceDelegateBridge::OnRequestCompleted(
    const network::SimpleURLLoader* source,
    const int response_code,
    std::optional<std::string> response_body,
    RemoteSuggestionsService::CompletionCallback completion_callback) {
  [delegate_ onRequestCompleted:source
                   responseCode:response_code
                   responseBody:std::move(response_body)
                     completion:std::move(completion_callback)];
}

void RemoteSuggestionsServiceDelegateBridge::OnIndexedRequestCompleted(
    const int request_index,
    const network::SimpleURLLoader* source,
    const int response_code,
    std::optional<std::string> response_body,
    RemoteSuggestionsService::IndexedCompletionCallback completion_callback) {
  [delegate_ onIndexedRequestCompleted:request_index
                             urlLoader:source
                          responseCode:response_code
                          responseBody:std::move(response_body)
                            completion:std::move(completion_callback)];
}
