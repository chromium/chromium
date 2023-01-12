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

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_loader_freeze_mode.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
struct ResourceRequest;
}  // namespace network

namespace blink {

class ResourceLoadInfoNotifierWrapper;
class WebBackForwardCacheLoaderHelper;
class WebBlobInfo;
class WebData;
class WebResourceRequestSender;
class WebURLRequestExtraData;
class WebURLLoaderClient;
class WebURLResponse;
struct WebURLError;

class BLINK_PLATFORM_EXPORT WebURLLoader {
 public:
  // When non-null |keep_alive_handle| is specified, this loader prolongs
  // this render process's lifetime.
  WebURLLoader(
      const WebVector<WebString>& cors_exempt_header_list,
      base::WaitableEvent* terminate_sync_load_event,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          freezable_task_runner_handle,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          unfreezable_task_runner_handle,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle,
      WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper);
  WebURLLoader(const WebURLLoader&) = delete;
  WebURLLoader& operator=(const WebURLLoader&) = delete;
  WebURLLoader();

  // The WebURLLoader may be deleted in a call to its client.
  virtual ~WebURLLoader();

  // Load the request synchronously, returning results directly to the
  // caller upon completion.  There is no mechanism to interrupt a
  // synchronous load!!
  // If the request's PassResponsePipeToClient flag is set to true, the response
  // will instead be redirected to a blob, which is passed out in
  // |downloaded_blob|.
  virtual void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient* client,
      WebURLResponse& response,
      absl::optional<WebURLError>& error,
      WebData& data,
      int64_t& encoded_data_length,
      uint64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper);

  // Load the request asynchronously, sending notifications to the given
  // client.  The client will receive no further notifications if the
  // loader is disposed before it completes its work.
  virtual void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client);

  // Freezes the loader. See blink/renderer/platform/loader/README.md for the
  // general concept of "freezing" in the loading module. See
  // blink/public/platform/web_loader_freezing_mode.h for `mode`.
  virtual void Freeze(WebLoaderFreezeMode mode);

  // Notifies the loader that the priority of a WebURLRequest has changed from
  // its previous value. For example, a preload request starts with low
  // priority, but may increase when the resource is needed for rendering.
  virtual void DidChangePriority(WebURLRequest::Priority new_priority,
                                 int intra_priority_value);

  // Returns the task runner for this request.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetTaskRunnerForBodyLoader();

  void SetResourceRequestSenderForTesting(
      std::unique_ptr<WebResourceRequestSender> resource_request_sender);

 private:
  class Context;
  class RequestPeerImpl;

  void Cancel();

  scoped_refptr<Context> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_H_
