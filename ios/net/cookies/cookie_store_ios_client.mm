// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios_client.h"
#import "base/task/sequenced_task_runner.h"

namespace {
// The CookieStoreIOSClient.
net::CookieStoreIOSClient* g_client;
}  // namespace

namespace net {

void SetCookieStoreIOSClient(CookieStoreIOSClient* client) {
  g_client = client;
}

CookieStoreIOSClient* GetCookieStoreIOSClient() {
  return g_client;
}

CookieStoreIOSClient::CookieStoreIOSClient() {}

CookieStoreIOSClient::~CookieStoreIOSClient() {}

scoped_refptr<base::SequencedTaskRunner>
CookieStoreIOSClient::GetTaskRunner() const {
  return scoped_refptr<base::SequencedTaskRunner>();
}

}  // namespace net
