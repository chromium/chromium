// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/chrome_cookie_store_ios_client.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

ChromeCookieStoreIOSClient::ChromeCookieStoreIOSClient() {}

scoped_refptr<base::SequencedTaskRunner>
ChromeCookieStoreIOSClient::GetTaskRunner() const {
  return web::GetIOThreadTaskRunner({});
}
