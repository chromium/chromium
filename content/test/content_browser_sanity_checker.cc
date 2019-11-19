// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_sanity_checker.h"

#include "base/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/test/web_contents_observer_sanity_checker.h"

namespace content {

namespace {
bool g_sanity_checks_already_enabled = false;
}

ContentBrowserSanityChecker::ContentBrowserSanityChecker() {
  CHECK(!g_sanity_checks_already_enabled)
      << "Tried to enable ContentBrowserSanityChecker, but it's already been "
      << "enabled.";
  g_sanity_checks_already_enabled = true;

  creation_hook_ =
      base::BindRepeating(&ContentBrowserSanityChecker::OnWebContentsCreated,
                          base::Unretained(this));
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(creation_hook_);
}

ContentBrowserSanityChecker::~ContentBrowserSanityChecker() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      creation_hook_);
  g_sanity_checks_already_enabled = false;
}

void ContentBrowserSanityChecker::OnWebContentsCreated(
    WebContents* web_contents) {
  WebContentsObserverSanityChecker::Enable(web_contents);
}

}  // namespace content
