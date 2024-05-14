// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/content/content_thread_impl.h"

// DCHECK_CURRENTLY_ON will be redefined in the content/ browser_thread.h
#undef DCHECK_CURRENTLY_ON

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread_delegate.h"

namespace web {

scoped_refptr<base::SingleThreadTaskRunner>
ContentThreadImpl::GetUIThreadTaskRunner(const WebTaskTraits& traits) {
  // Map WebTraits to browser traits. iOS doesn't use TaskPriorities in
  // WebThread so take a best guess.
  content::BrowserTaskTraits browser_traits{base::TaskPriority::BEST_EFFORT};
  return content::GetUIThreadTaskRunner(browser_traits);
}

scoped_refptr<base::SingleThreadTaskRunner>
ContentThreadImpl::GetIOThreadTaskRunner(const WebTaskTraits& traits) {
  // Map WebTraits to browser traits. iOS doesn't use TaskPriorities in
  // WebThread so take a best guess.
  content::BrowserTaskTraits browser_traits{base::TaskPriority::BEST_EFFORT};
  return content::GetIOThreadTaskRunner(browser_traits);
}

content::BrowserThread::ID MapWebToBrowserID(WebThread::ID identifier) {
  if (identifier == WebThread::UI) {
    return content::BrowserThread::UI;
  }
  if (identifier == WebThread::IO) {
    return content::BrowserThread::IO;
  }
  NOTREACHED_IN_MIGRATION();
  return content::BrowserThread::UI;  // default?
}

WebThread::ID MapBrowserToWebID(content::BrowserThread::ID identifier) {
  if (identifier == content::BrowserThread::UI) {
    return WebThread::UI;
  }
  if (identifier == content::BrowserThread::IO) {
    return WebThread::IO;
  }
  return WebThread::UI;
}

// static
bool ContentThreadImpl::IsThreadInitialized(ID identifier) {
  return content::BrowserThread::IsThreadInitialized(
      MapWebToBrowserID(identifier));
}

// static
bool ContentThreadImpl::CurrentlyOn(ID identifier) {
  return content::BrowserThread::CurrentlyOn(MapWebToBrowserID(identifier));
}

// static
std::string ContentThreadImpl::GetCurrentlyOnErrorMessage(ID expected) {
  return content::BrowserThread::GetCurrentlyOnErrorMessage(
      MapWebToBrowserID(expected));
}

// static
bool ContentThreadImpl::GetCurrentThreadIdentifier(ID* identifier) {
  content::BrowserThread::ID browser_id = content::BrowserThread::UI;
  bool result = content::BrowserThread::GetCurrentThreadIdentifier(&browser_id);
  *identifier = MapBrowserToWebID(browser_id);
  return result;
}

}  // namespace web
