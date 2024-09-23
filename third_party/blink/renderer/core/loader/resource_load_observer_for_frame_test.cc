// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/mock_content_security_notifier.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Tests that when a resource with certificate errors is loaded from the memory
// cache, the embedder is notified.
TEST(ResourceLoadObserverForFrameTest, MemoryCacheCertificateError) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(), nullptr, MakeGarbageCollected<EmptyLocalFrameClient>());
  LocalFrame& frame = dummy_page_holder->GetFrame();
  auto* observer = MakeGarbageCollected<ResourceLoadObserverForFrame>(
      *frame.GetDocument()->Loader(), *frame.GetDocument(),
      *MakeGarbageCollected<TestResourceFetcherProperties>());

  testing::StrictMock<MockContentSecurityNotifier> mock_notifier;
  base::ScopedClosureRunner clear_binder(WTF::BindOnce(
      [](LocalFrame* frame) {
        frame->GetBrowserInterfaceBroker().SetBinderForTesting(
            mojom::blink::ContentSecurityNotifier::Name_, {});
      },
      WrapWeakPersistent(&frame)));

  frame.GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::ContentSecurityNotifier::Name_,
      base::BindLambdaForTesting([&](mojo::ScopedMessagePipeHandle handle) {
        mock_notifier.Bind(
            mojo::PendingReceiver<mojom::blink::ContentSecurityNotifier>(
                std::move(handle)));
      }));

  KURL url("https://www.example.com/");
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
  ResourceResponse response(url);
  response.SetHasMajorCertificateErrors(true);
  auto* resource = MakeGarbageCollected<MockResource>(resource_request);
  resource->SetResponse(response);

  EXPECT_CALL(mock_notifier, NotifyContentWithCertificateErrorsDisplayed())
      .Times(1);
  observer->DidReceiveResponse(
      99, resource_request, resource->GetResponse(), resource,
      ResourceLoadObserver::ResponseSource::kFromMemoryCache);

  test::RunPendingTasks();
}

}  // namespace blink
