// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_

#include "content/public/browser/preloading_data.h"

namespace content {

// PrerenderHandle is the class used to encapsulate prerender attempt in
// content/.
// The lifetime of this instance is determined by the embedders which attempt to
// trigger prerendering, and they should release this instance once they no
// longer want to prerender the page. A PrerenderHandle is bound to a
// prerender attempt, which means the PrerenderHandle exists even if the
// triggering or prerendering failed. In its destructor, if any resources were
// allocated for this attempt, the resources will be released.
class PrerenderHandle {
 public:
  PrerenderHandle() = default;
  virtual ~PrerenderHandle() = default;

  // Returns the initial URL that is passed to PrerenderHostRegistry for
  // starting a prerendering page.
  virtual const GURL& GetInitialPrerenderingUrl() const = 0;

  // Returns true if a prerender attempt was successfully triggered, i.e.,
  // passed all pre-checks, e.g., eligibility checks. Note that returning true
  // does not mean the prerendering page is successfully prerendered and ready
  // for activation.
  virtual bool WasSuccessfullyTriggeredForTesting() const = 0;

  virtual base::WeakPtr<PrerenderHandle> GetWeakPtr() = 0;
  virtual void SetPreloadingAttemptFailureReason(
      PreloadingFailureReason reason) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
