// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_test_util.h"

namespace blink {

FetchLaterTestingScope::FetchLaterTestingScope(LocalFrameClient* frame_client,
                                               const String& source_page_url)
    : V8TestingScope(DummyPageHolder::CreateAndCommitNavigation(
          KURL(source_page_url),
          /*initial_view_size=*/gfx::Size(),
          /*chrome_client=*/nullptr,
          frame_client)) {}

void MockFetchLaterLoaderFactory::CreateLoader(
    mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  ++num_create_loader_calls_;
  // TODO(crbug.com/478888135): Make this `ResourceRequest` copying explicit.
  create_loader_resource_request_ = resource_request;
}

const network::ResourceRequest&
MockFetchLaterLoaderFactory::GetCreateLoaderResourceRequest() const {
  CHECK_GT(NumberOfCreateLoaderCalls(), 0);
  return create_loader_resource_request_;
}

}  // namespace blink
