// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_FRAME_REQUEST_BLOCKER_H_
#define CONTENT_RENDERER_LOADER_FRAME_REQUEST_BLOCKER_H_

#include "base/atomic_ref_count.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"

namespace blink {
class URLLoaderThrottle;
}

namespace content {

// Allows the browser to block and then resume requests from a frame. This
// includes requests from the frame's dedicated workers as well.
// This class is thread-safe because it can be used on multiple threads, for
// example by sync XHRs and dedicated workers.
// TODO(crbug.com/581037): once committed interstitials launch, the remaining
// use cases should be switched to pause the frame request in the browser and
// this code can be removed.
class FrameRequestBlocker
    : public base::RefCountedThreadSafe<FrameRequestBlocker> {
 public:
  FrameRequestBlocker();

  // Block any new subresource requests.
  void Block();

  // Resumes any blocked subresource requests.
  void Resume();

  // Cancels any blocked subresource requests.
  void Cancel();

  std::unique_ptr<blink::URLLoaderThrottle> GetThrottleIfRequestsBlocked();

 private:
  class Client {
   public:
    virtual void Resume() = 0;
    virtual void Cancel() = 0;
  };

  friend class base::RefCountedThreadSafe<FrameRequestBlocker>;
  friend class RequestBlockerThrottle;
  virtual ~FrameRequestBlocker();

  bool RegisterClientIfRequestsBlocked(Client* client);

  void RemoveObserver(Client* client);

  scoped_refptr<base::ObserverListThreadSafe<Client>> clients_;

  base::AtomicRefCount blocked_;

  DISALLOW_COPY_AND_ASSIGN(FrameRequestBlocker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_FRAME_REQUEST_BLOCKER_H_
