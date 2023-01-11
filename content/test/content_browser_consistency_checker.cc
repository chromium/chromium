// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_consistency_checker.h"

#include "base/functional/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
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

  creation_subscription_ =
      RegisterWebContentsCreationCallback(base::BindRepeating(
          &ContentBrowserConsistencyChecker::OnWebContentsCreated,
          base::Unretained(this)));
}

ContentBrowserConsistencyChecker::~ContentBrowserConsistencyChecker() {
  g_consistency_checks_already_enabled = false;
}

void ContentBrowserConsistencyChecker::OnWebContentsCreated(
    WebContents* web_contents) {
  WebContentsObserverConsistencyChecker::Enable(web_contents);
}

}  // namespace content
