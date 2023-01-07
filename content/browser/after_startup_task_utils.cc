// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/after_startup_task_utils.h"

#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

void SetBrowserStartupIsCompleteForTesting() {
  content::BrowserTaskExecutor::OnStartupComplete();
  // Forward the message to ContentBrowserClient if one is registered (there are
  // many tests where one isn't but that's fine as that also means they get the
  // default ContentBrowserClient::IsBrowserStartupComplete() which is always
  // true).
  ContentClient* content_client = GetContentClient();
  if (content_client) {
    ContentBrowserClient* content_browser_client = content_client->browser();
    if (content_browser_client)
      content_browser_client->SetBrowserStartupIsCompleteForTesting();
  }
}

}  // namespace content
