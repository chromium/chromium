// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_child_process_observer.h"

#include "content/browser/browser_child_process_host_impl.h"

namespace content {

// static
void BrowserChildProcessObserver::Add(BrowserChildProcessObserver* observer) {
  BrowserChildProcessHostImpl::AddObserver(observer);
}

// static
void BrowserChildProcessObserver::Remove(
    BrowserChildProcessObserver* observer) {
  BrowserChildProcessHostImpl::RemoveObserver(observer);
}

}  // namespace content
