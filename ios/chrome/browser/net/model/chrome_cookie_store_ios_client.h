// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_CHROME_COOKIE_STORE_IOS_CLIENT_H_
#define IOS_CHROME_BROWSER_NET_MODEL_CHROME_COOKIE_STORE_IOS_CLIENT_H_

#include "base/task/sequenced_task_runner.h"
#include "ios/net/cookies/cookie_store_ios_client.h"

// Chrome implementation of net::CookieStoreIOSClient. This class lives on the
// IOThread.
class ChromeCookieStoreIOSClient : public net::CookieStoreIOSClient {
 public:
  ChromeCookieStoreIOSClient();

  ChromeCookieStoreIOSClient(const ChromeCookieStoreIOSClient&) = delete;
  ChromeCookieStoreIOSClient& operator=(const ChromeCookieStoreIOSClient&) =
      delete;

  // CookieStoreIOSClient implementation.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_CHROME_COOKIE_STORE_IOS_CLIENT_H_
