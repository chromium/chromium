// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"

namespace blink {

// Tests that when a resource with certificate errors is loaded from the memory
// cache, the embedder is notified.
TEST(ResourceLoadObserverForFrameTest, MemoryCacheCertificateError) {
  class MockFrameClient final : public EmptyLocalFrameClient {
   public:
    void DidDisplayContentWithCertificateErrors() override {
      did_display_content_with_certificate_errors_called_ = true;
    }

    bool IsDidDisplayContentWithCertificateErrorsCalled() const {
      return did_display_content_with_certificate_errors_called_;
    }

   private:
    bool did_display_content_with_certificate_errors_called_ = false;
  };

  auto* client = MakeGarbageCollected<MockFrameClient>();
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(IntSize(), nullptr, client);
  LocalFrame& frame = dummy_page_holder->GetFrame();
  auto* observer = MakeGarbageCollected<ResourceLoadObserverForFrame>(
      *MakeGarbageCollected<FrameOrImportedDocument>(
          *frame.GetDocument()->Loader(), *frame.GetDocument()),
      *MakeGarbageCollected<TestResourceFetcherProperties>());
  KURL url("https://www.example.com/");
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
  ResourceResponse response(url);
  response.SetHasMajorCertificateErrors(true);
  auto* resource = MakeGarbageCollected<MockResource>(resource_request);
  resource->SetResponse(response);

  EXPECT_FALSE(client->IsDidDisplayContentWithCertificateErrorsCalled());
  observer->DidReceiveResponse(
      99, resource_request, resource->GetResponse(), resource,
      ResourceLoadObserver::ResponseSource::kFromMemoryCache);
  EXPECT_TRUE(client->IsDidDisplayContentWithCertificateErrorsCalled());
}

}  // namespace blink
