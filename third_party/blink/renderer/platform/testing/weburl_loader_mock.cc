// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/weburl_loader_mock.h"

#include <utility>

#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/weburl_loader_mock_factory_impl.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

WebURLLoaderMock::WebURLLoaderMock(WebURLLoaderMockFactoryImpl* factory)
    : factory_(factory) {}

WebURLLoaderMock::~WebURLLoaderMock() {
  Cancel();
}

void WebURLLoaderMock::ServeAsynchronousRequest(
    WebURLLoaderTestDelegate* delegate,
    const WebURLResponse& response,
    const WebData& data,
    const absl::optional<WebURLError>& error) {
  if (!client_)
    return;

  // If no delegate is provided then create an empty one. The default behavior
  // will just proxy to the client.
  std::unique_ptr<WebURLLoaderTestDelegate> default_delegate;
  if (!delegate) {
    default_delegate = std::make_unique<WebURLLoaderTestDelegate>();
    delegate = default_delegate.get();
  }

  if (error) {
    delegate->DidFail(client_, *error, data.size(), 0, 0);
    return;
  }

  // didReceiveResponse() and didReceiveData() might end up getting ::cancel()
  // to be called which will make the ResourceLoader to delete |this|.
  base::WeakPtr<WebURLLoaderMock> self = weak_factory_.GetWeakPtr();

  delegate->DidReceiveResponse(client_, response);
  if (!self)
    return;

  data.ForEachSegment([this, &delegate, &self](const char* segment,
                                               size_t segment_size,
                                               size_t segment_offset) {
    delegate->DidReceiveData(client_, segment,
                             base::checked_cast<int>(segment_size));
    // DidReceiveData() may clear the |self| weak ptr.  We stop iterating
    // when that happens.
    return self;
  });

  if (!self)
    return;

  delegate->DidFinishLoading(client_, base::TimeTicks(), data.size(),
                             data.size(), data.size());
}

WebURL WebURLLoaderMock::ServeRedirect(
    const WebString& method,
    const WebURLResponse& redirect_response) {
  KURL redirect_url(redirect_response.HttpHeaderField("Location"));

  base::WeakPtr<WebURLLoaderMock> self = weak_factory_.GetWeakPtr();

  bool report_raw_headers = false;
  bool follow = client_->WillFollowRedirect(
      redirect_url, net::SiteForCookies::FromUrl(GURL(redirect_url)),
      WebString(), network::mojom::ReferrerPolicy::kDefault, method,
      redirect_response, report_raw_headers, nullptr /* removed_headers */,
      false /* insecure_scheme_was_upgraded */);
  // |this| might be deleted in willFollowRedirect().
  if (!self)
    return redirect_url;

  if (!follow)
    Cancel();

  return redirect_url;
}

void WebURLLoaderMock::LoadSynchronously(
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
    int64_t& encoded_body_length,
    blink::WebBlobInfo& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  DCHECK(factory_->IsMockedURL(WebURL(KURL(request->url)))) << request->url;
  factory_->LoadSynchronously(std::move(request), &response, &error, &data,
                              &encoded_data_length);
}

void WebURLLoaderMock::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    WebURLLoaderClient* client) {
  DCHECK(client);
  DCHECK(factory_->IsMockedURL(WebURL(KURL(request->url)))) << request->url;
  client_ = client;
  factory_->LoadAsynchronouly(std::move(request), this);
}

void WebURLLoaderMock::Cancel() {
  client_ = nullptr;
  factory_->CancelLoad(this);
}

void WebURLLoaderMock::Freeze(WebLoaderFreezeMode mode) {
  is_deferred_ = (mode != WebLoaderFreezeMode::kNone);
  // Ignores setDefersLoading(false) safely.
  if (!is_deferred_)
    return;

  // setDefersLoading(true) is not implemented.
  NOTIMPLEMENTED();
}

void WebURLLoaderMock::DidChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {}

scoped_refptr<base::SingleThreadTaskRunner>
WebURLLoaderMock::GetTaskRunnerForBodyLoader() {
  return base::MakeRefCounted<scheduler::FakeTaskRunner>();
}

base::WeakPtr<WebURLLoaderMock> WebURLLoaderMock::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

} // namespace blink
