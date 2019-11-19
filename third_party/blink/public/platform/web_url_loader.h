/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_H_

#include <stdint.h>
#include "base/optional.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class WebData;
class WebURLLoaderClient;
class WebURLResponse;
struct WebURLError;

class WebURLLoader {
 public:
  // The WebURLLoader may be deleted in a call to its client.
  virtual ~WebURLLoader() = default;

  // Load the request synchronously, returning results directly to the
  // caller upon completion.  There is no mechanism to interrupt a
  // synchronous load!!
  // If the request's PassResponsePipeToClient flag is set to true, the response
  // will instead be redirected to a blob, which is passed out in
  // |downloaded_blob|.
  virtual void LoadSynchronously(
      const WebURLRequest&,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob) = 0;

  // Load the request asynchronously, sending notifications to the given
  // client.  The client will receive no further notifications if the
  // loader is disposed before it completes its work.
  virtual void LoadAsynchronously(const WebURLRequest&,
                                  WebURLLoaderClient*) = 0;

  // Suspends/resumes an asynchronous load.
  virtual void SetDefersLoading(bool) = 0;

  // Notifies the loader that the priority of a WebURLRequest has changed from
  // its previous value. For example, a preload request starts with low
  // priority, but may increase when the resource is needed for rendering.
  virtual void DidChangePriority(WebURLRequest::Priority new_priority,
                                 int intra_priority_value) = 0;

  // Returns the task runner for this request.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;
};

}  // namespace blink

#endif
