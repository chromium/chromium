// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_fetcher_consumer_bridge.h"

FormFetcherConsumerBridge::FormFetcherConsumerBridge(
    id<FormFetcherConsumer> delegate,
    password_manager::FormFetcher* fetcher)
    : delegate_(delegate) {
  CHECK(fetcher);
  fetcher->AddConsumer(this);
}

FormFetcherConsumerBridge::~FormFetcherConsumerBridge() = default;

void FormFetcherConsumerBridge::OnFetchCompleted() {
  [delegate_ fetchDidComplete];
}
