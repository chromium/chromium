// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_CONTENTS_OBSERVER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_WEB_CONTENTS_OBSERVER_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// This file contains helpers wrapping WebContentsObserver methods to allow
// tests to pass base::Callbacks to listen to the events (which can be defined
// inline and capture the test context using BindLambdaForTesting) rather than
// having to subclass WebContentsObserver.

// Forwards DidStartNavigation calls to the provided callback.
class NavigationStartObserver : public WebContentsObserver {
 public:
  using Callback = base::RepeatingCallback<void(NavigationHandle*)>;

  NavigationStartObserver(WebContents* web_contents, Callback callback);
  ~NavigationStartObserver() override;

 private:
  void DidStartNavigation(NavigationHandle* navigation) override;

  Callback callback_;
};

// Forwards DidFinishNavigation calls to the provided callback.
class NavigationFinishObserver : public WebContentsObserver {
 public:
  using Callback = base::RepeatingCallback<void(NavigationHandle*)>;

  NavigationFinishObserver(WebContents* web_contents, Callback callback);
  ~NavigationFinishObserver() override;

 private:
  void DidFinishNavigation(NavigationHandle* navigation) override;

  Callback callback_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_CONTENTS_OBSERVER_TEST_UTILS_H_
