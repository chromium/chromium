// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FRAME_REQUEST_BLOCKER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FRAME_REQUEST_BLOCKER_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class URLLoaderThrottle;

// Allows the browser to block and then resume requests from a frame. This
// includes requests from the frame's dedicated workers as well.
// This class is thread-safe because it can be used on multiple threads, for
// example by sync XHRs and dedicated workers.
// TODO(crbug.com/581037): once committed interstitials launch, the remaining
// use cases should be switched to pause the frame request in the browser and
// this code can be removed.
class BLINK_PLATFORM_EXPORT WebFrameRequestBlocker
    : public base::RefCountedThreadSafe<WebFrameRequestBlocker> {
 public:
  virtual ~WebFrameRequestBlocker() = default;

  static scoped_refptr<WebFrameRequestBlocker> Create();

  // Block any new subresource requests.
  virtual void Block() = 0;

  // Resumes any blocked subresource requests.
  virtual void Resume() = 0;

  virtual std::unique_ptr<URLLoaderThrottle> GetThrottleIfRequestsBlocked() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FRAME_REQUEST_BLOCKER_H_
