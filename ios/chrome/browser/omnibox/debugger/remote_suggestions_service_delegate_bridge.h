// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_REMOTE_SUGGESTIONS_SERVICE_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_REMOTE_SUGGESTIONS_SERVICE_DELEGATE_BRIDGE_H_

#import <memory>
#import <optional>
#import <string>

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/remote_suggestions_service.h"

@protocol RemoteSuggestionsServiceDelegate <NSObject>

- (void)onRequestCompleted:(const network::SimpleURLLoader*)source
              responseCode:(int)responseCode
              responseBody:(std::optional<std::string>)responseBody
                completion:
                    (RemoteSuggestionsService::CompletionCallback)completion;

- (void)onIndexedRequestCompleted:(int)requestIndex
                        urlLoader:(const network::SimpleURLLoader*)source
                     responseCode:(int)responseCode
                     responseBody:(std::optional<std::string>)responseBody
                       completion:
                           (RemoteSuggestionsService::IndexedCompletionCallback)
                               completion;

@end

class RemoteSuggestionsServiceDelegateBridge
    : public RemoteSuggestionsService::Delegate {
 public:
  RemoteSuggestionsServiceDelegateBridge(
      id<RemoteSuggestionsServiceDelegate> delegate,
      RemoteSuggestionsService* remote_suggestions_service);

  ~RemoteSuggestionsServiceDelegateBridge() override;

  RemoteSuggestionsServiceDelegateBridge(
      const RemoteSuggestionsServiceDelegateBridge&) = delete;
  RemoteSuggestionsServiceDelegateBridge& operator=(
      const RemoteSuggestionsServiceDelegateBridge&) = delete;

  base::WeakPtr<RemoteSuggestionsServiceDelegateBridge> AsWeakPtr();

  void OnRequestCompleted(const network::SimpleURLLoader* source,
                          const int response_code,
                          std::optional<std::string> response_body,
                          RemoteSuggestionsService::CompletionCallback
                              completion_callback) override;

  void OnIndexedRequestCompleted(
      const int request_index,
      const network::SimpleURLLoader* source,
      const int response_code,
      std::optional<std::string> response_body,
      RemoteSuggestionsService::IndexedCompletionCallback completion_callback)
      override;

 private:
  __weak id<RemoteSuggestionsServiceDelegate> delegate_;
  raw_ptr<RemoteSuggestionsService> remote_suggestions_service_;
  base::WeakPtrFactory<RemoteSuggestionsServiceDelegateBridge>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_REMOTE_SUGGESTIONS_SERVICE_DELEGATE_BRIDGE_H_
