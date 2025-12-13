// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_DELEGATE_H_

// Delegate for the PrerenderTabHelper.
class PrerenderTabHelperDelegate {
 public:
  PrerenderTabHelperDelegate() = default;

  PrerenderTabHelperDelegate(const PrerenderTabHelperDelegate&) = delete;
  PrerenderTabHelperDelegate& operator=(const PrerenderTabHelperDelegate&) =
      delete;

  virtual ~PrerenderTabHelperDelegate() = default;

  // Cancel the prerender.
  virtual void CancelPrerender() = 0;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_DELEGATE_H_
