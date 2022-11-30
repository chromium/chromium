// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FRAME_REQUEST_BLOCKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FRAME_REQUEST_BLOCKER_H_

#include "base/atomic_ref_count.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "third_party/blink/public/platform/web_frame_request_blocker.h"

namespace blink {

// This is the implementation of WebFrameRequestBlocker. See comments in
// third_party/blink/public/platform/web_frame_request_blocker.h for details.
class FrameRequestBlocker final : public WebFrameRequestBlocker {
 public:
  FrameRequestBlocker();
  FrameRequestBlocker(const FrameRequestBlocker&) = delete;
  FrameRequestBlocker& operator=(const FrameRequestBlocker&) = delete;

  // Block any new subresource requests.
  void Block() override;

  // Resumes any blocked subresource requests.
  void Resume() override;

  std::unique_ptr<URLLoaderThrottle> GetThrottleIfRequestsBlocked() override;

 private:
  class Client {
   public:
    virtual void Resume() = 0;
  };

  friend class base::RefCountedThreadSafe<FrameRequestBlocker>;
  friend class RequestBlockerThrottle;
  ~FrameRequestBlocker() override;

  bool RegisterClientIfRequestsBlocked(Client* client);

  void RemoveObserver(Client* client);

  scoped_refptr<base::ObserverListThreadSafe<Client>> clients_;

  base::AtomicRefCount blocked_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FRAME_REQUEST_BLOCKER_H_
