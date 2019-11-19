// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/weburl_loader_mock.h"

#include <utility>

#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
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
    const base::Optional<WebURLError>& error) {
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
    delegate->DidReceiveData(client_, segment, segment_size);
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
    const WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  KURL redirect_url(redirect_response.HttpHeaderField("Location"));

  base::WeakPtr<WebURLLoaderMock> self = weak_factory_.GetWeakPtr();

  bool report_raw_headers = false;
  bool follow = client_->WillFollowRedirect(
      redirect_url, redirect_url, WebString(),
      network::mojom::ReferrerPolicy::kDefault, request.HttpMethod(),
      redirect_response, report_raw_headers);
  // |this| might be deleted in willFollowRedirect().
  if (!self)
    return redirect_url;

  if (!follow)
    Cancel();

  return redirect_url;
}

void WebURLLoaderMock::LoadSynchronously(
    const WebURLRequest& request,
    WebURLLoaderClient* client,
    WebURLResponse& response,
    base::Optional<WebURLError>& error,
    WebData& data,
    int64_t& encoded_data_length,
    int64_t& encoded_body_length,
    blink::WebBlobInfo& downloaded_blob) {
  DCHECK(factory_->IsMockedURL(request.Url()));
  factory_->LoadSynchronously(request, &response, &error, &data,
                              &encoded_data_length);
}

void WebURLLoaderMock::LoadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  DCHECK(client);
  DCHECK(factory_->IsMockedURL(request.Url()));
  client_ = client;
  factory_->LoadAsynchronouly(request, this);
}

void WebURLLoaderMock::Cancel() {
  client_ = nullptr;
  factory_->CancelLoad(this);
}

void WebURLLoaderMock::SetDefersLoading(bool deferred) {
  is_deferred_ = deferred;
  // Ignores setDefersLoading(false) safely.
  if (!deferred)
    return;

  // setDefersLoading(true) is not implemented.
  NOTIMPLEMENTED();
}

void WebURLLoaderMock::DidChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {}

scoped_refptr<base::SingleThreadTaskRunner> WebURLLoaderMock::GetTaskRunner() {
  return base::MakeRefCounted<scheduler::FakeTaskRunner>();
}

base::WeakPtr<WebURLLoaderMock> WebURLLoaderMock::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

} // namespace blink
