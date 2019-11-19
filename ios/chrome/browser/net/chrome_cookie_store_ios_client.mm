// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/chrome_cookie_store_ios_client.h"

#include "base/task/post_task.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeCookieStoreIOSClient::ChromeCookieStoreIOSClient() {}

scoped_refptr<base::SequencedTaskRunner>
ChromeCookieStoreIOSClient::GetTaskRunner() const {
  return base::CreateSingleThreadTaskRunner({web::WebThread::IO});
}
