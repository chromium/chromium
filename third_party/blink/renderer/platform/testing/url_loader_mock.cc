// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/url_loader_mock.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory_impl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

URLLoaderMock::URLLoaderMock(URLLoaderMockFactoryImpl* factory)
    : factory_(factory) {}

URLLoaderMock::~URLLoaderMock() {
  Cancel();
}

void URLLoaderMock::ServeAsynchronousRequest(
    URLLoaderTestDelegate* delegate,
    const WebURLResponse& response,
    const scoped_refptr<SharedBuffer>& data,
    const std::optional<WebURLError>& error) {
  if (!client_) {
    return;
  }
  size_t data_size = data ? data->size() : 0;

  // If no delegate is provided then create an empty one. The default behavior
  // will just proxy to the client.
  std::unique_ptr<URLLoaderTestDelegate> default_delegate;
  if (!delegate) {
    default_delegate = std::make_unique<URLLoaderTestDelegate>();
    delegate = default_delegate.get();
  }

  if (error) {
    delegate->DidFail(client_, *error, data_size, 0, 0);
    return;
  }

  // didReceiveResponse() and didReceiveData() might end up getting ::cancel()
  // to be called which will make the ResourceLoader to delete |this|.
  base::WeakPtr<URLLoaderMock> self = weak_factory_.GetWeakPtr();

  delegate->DidReceiveResponse(client_, response);
  if (!self) {
    return;
  }

  if (data) {
    for (const auto& span : *data) {
      delegate->DidReceiveData(client_, span);
      // DidReceiveData() may clear the |self| weak ptr.  We stop iterating
      // when that happens.
      if (!self) {
        return;
      }
    }
  }

  delegate->DidFinishLoading(client_, base::TimeTicks(), data_size, data_size,
                             data_size);
}

WebURL URLLoaderMock::ServeRedirect(const WebString& method,
                                    const WebURLResponse& redirect_response) {
  KURL redirect_url(redirect_response.ResponseUrl(),
                    redirect_response.HttpHeaderField(http_names::kLocation));

  base::WeakPtr<URLLoaderMock> self = weak_factory_.GetWeakPtr();

  bool report_raw_headers = false;
  net::HttpRequestHeaders modified_headers;
  bool follow = client_->WillFollowRedirect(
      redirect_url, net::SiteForCookies::FromUrl(GURL(redirect_url)),
      WebString(), network::mojom::ReferrerPolicy::kDefault, method,
      redirect_response, report_raw_headers, nullptr /* removed_headers */,
      modified_headers, false /* insecure_scheme_was_upgraded */);
  // |this| might be deleted in willFollowRedirect().
  if (!self) {
    return redirect_url;
  }

  if (!follow) {
    Cancel();
  }

  return redirect_url;
}

void URLLoaderMock::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient* client,
    WebURLResponse& response,
    std::optional<WebURLError>& error,
    scoped_refptr<SharedBuffer>& data,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  DCHECK(factory_->IsMockedURL(WebURL(KURL(request->url)))) << request->url;
  factory_->LoadSynchronously(std::move(request), &response, &error, data,
                              &encoded_data_length);
}

void URLLoaderMock::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    URLLoaderClient* client) {
  DCHECK(client);
  DCHECK(factory_->IsMockedURL(WebURL(KURL(request->url)))) << request->url;
  client_ = client;
  factory_->LoadAsynchronouly(std::move(request), this);
}

void URLLoaderMock::Cancel() {
  client_ = nullptr;
  factory_->CancelLoad(this);
}

void URLLoaderMock::Freeze(LoaderFreezeMode mode) {
  is_deferred_ = (mode != LoaderFreezeMode::kNone);
  // Ignores setDefersLoading(false) safely.
  if (!is_deferred_) {
    return;
  }

  // setDefersLoading(true) is not implemented.
  NOTIMPLEMENTED();
}

void URLLoaderMock::DidChangePriority(WebURLRequest::Priority new_priority,
                                      int intra_priority_value) {}

scoped_refptr<base::SingleThreadTaskRunner>
URLLoaderMock::GetTaskRunnerForBodyLoader() {
  return base::MakeRefCounted<scheduler::FakeTaskRunner>();
}

base::WeakPtr<URLLoaderMock> URLLoaderMock::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
