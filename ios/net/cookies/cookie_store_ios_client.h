// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"

namespace net {

class CookieStoreIOSClient;

// Setter and getter for the client.
void SetCookieStoreIOSClient(CookieStoreIOSClient* client);
CookieStoreIOSClient* GetCookieStoreIOSClient();

// Interface that the embedder of the net layer implements. This class lives on
// the same thread as the CookieStoreIOS.
class CookieStoreIOSClient {
 public:
  CookieStoreIOSClient();

  CookieStoreIOSClient(const CookieStoreIOSClient&) = delete;
  CookieStoreIOSClient& operator=(const CookieStoreIOSClient&) = delete;

  virtual ~CookieStoreIOSClient();

  // Returns instance of SequencedTaskRunner used for blocking file I/O.
  virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_
