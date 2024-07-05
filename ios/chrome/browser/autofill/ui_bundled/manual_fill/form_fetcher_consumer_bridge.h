// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_FETCHER_CONSUMER_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_FETCHER_CONSUMER_BRIDGE_H_

#import "components/password_manager/core/browser/form_fetcher_impl.h"

// Objective-C protocol mirroring FormFetcher::Consumer.
@protocol FormFetcherConsumer

// Called when the FormFetcher has finished fetching passwords.
- (void)fetchDidComplete;

@end

// Simple consumer bridge that forwards all events to its delegate.
class FormFetcherConsumerBridge
    : public password_manager::FormFetcher::Consumer {
 public:
  // `form_fetcher` must not be null and must outlive `this`.
  FormFetcherConsumerBridge(id<FormFetcherConsumer> delegate,
                            password_manager::FormFetcher* form_fetcher);
  ~FormFetcherConsumerBridge() override;

  // FormFetcher::Consumer.
  void OnFetchCompleted() override;

 private:
  __weak id<FormFetcherConsumer> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_FETCHER_CONSUMER_BRIDGE_H_
