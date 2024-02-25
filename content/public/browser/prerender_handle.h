// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_

#include "content/public/browser/preloading_data.h"

namespace content {

// PrerenderHandle is the class used to encapsulate prerender resources in
// content/. In its destructor, the resource is expected to be released.
class PrerenderHandle {
 public:
  PrerenderHandle() = default;
  virtual ~PrerenderHandle() = default;

  // Returns the initial URL that is passed to PrerenderHostRegistry for
  // starting a prerendering page.
  virtual const GURL& GetInitialPrerenderingUrl() const = 0;
  virtual base::WeakPtr<PrerenderHandle> GetWeakPtr() = 0;
  virtual void SetPreloadingAttemptFailureReason(
      PreloadingFailureReason reason) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
