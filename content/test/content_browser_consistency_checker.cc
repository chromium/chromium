// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_consistency_checker.h"

#include "base/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/test/web_contents_observer_consistency_checker.h"

namespace content {

namespace {
bool g_consistency_checks_already_enabled = false;
}

ContentBrowserConsistencyChecker::ContentBrowserConsistencyChecker() {
  CHECK(!g_consistency_checks_already_enabled)
      << "Tried to enable ContentBrowserConsistencyChecker, but it's already "
      << "been enabled.";
  g_consistency_checks_already_enabled = true;

  creation_hook_ = base::BindRepeating(
      &ContentBrowserConsistencyChecker::OnWebContentsCreated,
      base::Unretained(this));
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(creation_hook_);
}

ContentBrowserConsistencyChecker::~ContentBrowserConsistencyChecker() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      creation_hook_);
  g_consistency_checks_already_enabled = false;
}

void ContentBrowserConsistencyChecker::OnWebContentsCreated(
    WebContents* web_contents) {
  WebContentsObserverConsistencyChecker::Enable(web_contents);
}

}  // namespace content
