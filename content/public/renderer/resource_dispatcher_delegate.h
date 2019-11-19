// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_
#define CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

class GURL;

namespace content {

class RequestPeer;

// Interface that allows observing request events and optionally replacing
// the peer. Note that if it doesn't replace the peer it must return the
// current peer so that the ownership is continued to be held by
// ResourceDispatcher.
class CONTENT_EXPORT ResourceDispatcherDelegate {
 public:
  virtual ~ResourceDispatcherDelegate() {}

  virtual void OnRequestComplete() = 0;

  // Note that |url|, |referrer| and |method| are the final values (e.g. after
  // any redirects).
  virtual std::unique_ptr<RequestPeer> OnReceivedResponse(
      std::unique_ptr<RequestPeer> current_peer,
      const std::string& mime_type,
      const GURL& url) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_
