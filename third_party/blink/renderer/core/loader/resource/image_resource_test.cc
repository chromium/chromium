/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/loader/resource/image_resource.h"

#include <memory>
#include <string_view>

#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/mock_image_resource_observer.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

using test::ScopedMockedURLLoad;

namespace {

// An image of size 1x1.
constexpr unsigned char kJpegImage[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x13,
    0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68,
    0x20, 0x47, 0x49, 0x4d, 0x50, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x05, 0x03,
    0x04, 0x04, 0x04, 0x03, 0x05, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06,
    0x07, 0x0c, 0x08, 0x07, 0x07, 0x07, 0x07, 0x0f, 0x0b, 0x0b, 0x09, 0x0c,
    0x11, 0x0f, 0x12, 0x12, 0x11, 0x0f, 0x11, 0x11, 0x13, 0x16, 0x1c, 0x17,
    0x13, 0x14, 0x1a, 0x15, 0x11, 0x11, 0x18, 0x21, 0x18, 0x1a, 0x1d, 0x1d,
    0x1f, 0x1f, 0x1f, 0x13, 0x17, 0x22, 0x24, 0x22, 0x1e, 0x24, 0x1c, 0x1e,
    0x1f, 0x1e, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x05, 0x05, 0x05, 0x07, 0x06,
    0x07, 0x0e, 0x08, 0x08, 0x0e, 0x1e, 0x14, 0x11, 0x14, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0xff,
    0xc0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03, 0x01, 0x22, 0x00,
    0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x15, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f,
    0x00, 0xb2, 0xc0, 0x07, 0xff, 0xd9};

constexpr int kJpegImageWidth = 1;
constexpr int kJpegImageHeight = 1;

constexpr size_t kJpegImageSubrangeWithDimensionsLength =
    sizeof(kJpegImage) - 1;
constexpr size_t kJpegImageSubrangeWithoutDimensionsLength = 3;

class ImageResourceTest : public testing::Test,
                          private ScopedMockOverlayScrollbars {
  void TearDown() override {
    // Trigger a GC so MockFinishObserver gets destroyed and EXPECT_CALL gets
    // checked before the test ends.
    ThreadState::Current()->CollectAllGarbageForTesting(
        ThreadState::StackState::kNoHeapPointers);
  }

 private:
  test::TaskEnvironment task_environment_;
};

// Ensure that the image decoder can determine the dimensions of kJpegImage from
// just the first kJpegImageSubrangeWithDimensionsLength bytes. If this test
// fails, then the test data here probably needs to be updated.
TEST_F(ImageResourceTest, DimensionsDecodableFromPartialTestImage) {
  scoped_refptr<Image> image = BitmapImage::Create();
  EXPECT_EQ(
      Image::kSizeAvailable,
      image->SetData(SharedBuffer::Create(
                         kJpegImage, kJpegImageSubrangeWithDimensionsLength),
                     true));
  EXPECT_TRUE(IsA<BitmapImage>(image.get()));
  EXPECT_EQ(1, image->width());
  EXPECT_EQ(1, image->height());
}

// An image of size 50x50.
constexpr unsigned char kJpegImage2[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03,
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x15, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03,
    0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xd9};

constexpr std::string_view kSvgImage =
    "<svg width=\"200\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"100px\" height=\"100px\" fill=\"red\"/>"
    "</svg>";

constexpr std::string_view kSvgImage2 =
    "<svg width=\"300\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"200px\" height=\"200px\" fill=\"green\"/>"
    "</svg>";

constexpr char kTestURL[] = "http://www.test.com/cancelTest.html";

String GetTestFilePath() {
  return test::CoreTestDataPath("cancelTest.html");
}

constexpr std::string_view kSvgImageWithSubresource =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"198\" height=\"100\">"
    "<style>"
    "  <![CDATA[@font-face{font-family:\"test\"; "
    "    src:url('data:font/ttf;base64,invalidFontData');}]]>"
    "</style>"
    "<text x=\"50\" y=\"50\" font-family=\"test\" font-size=\"16\">Fox</text>"
    "</svg>";

void ReceiveResponse(ImageResource* image_resource,
                     const KURL& url,
                     const char* mime_type,
                     base::span<const char> data) {
  ResourceResponse resource_response(url);
  resource_response.SetMimeType(AtomicString(mime_type));
  resource_response.SetHttpStatusCode(200);
  image_resource->NotifyStartLoad();
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(data);
  image_resource->FinishForTest();
}

AtomicString BuildContentRange(size_t range_length, size_t total_length) {
  return AtomicString(String("bytes 0-" + String::Number(range_length - 1) +
                             "/" + String::Number(total_length)));
}

ResourceFetcher* CreateFetcher() {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), MakeGarbageCollected<MockFetchContext>(),
      base::MakeRefCounted<scheduler::FakeTaskRunner>(),
      base::MakeRefCounted<scheduler::FakeTaskRunner>(),
      MakeGarbageCollected<TestLoaderFactory>(),
      MakeGarbageCollected<MockContextLifecycleNotifier>(),
      nullptr /* back_forward_cache_loader_helper */));
}

TEST_F(ImageResourceTest, MultipartImage) {
  ResourceFetcher* fetcher = CreateFetcher();
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  // Emulate starting a real load, but don't expect any "real"
  // URLLoaderClient callbacks.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);
  fetcher->StartLoad(image_resource);

  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // Send the multipart response. No image or data buffer is created. Note that
  // the response must be routed through ResourceLoader to ensure the load is
  // flagged as multipart.
  ResourceResponse multipart_response(NullURL());
  multipart_response.SetMimeType(AtomicString("multipart/x-mixed-replace"));
  multipart_response.SetHttpHeaderField(
      http_names::kContentType,
      AtomicString("multipart/x-mixed-replace; boundary=boundary"));
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(multipart_response),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);
  EXPECT_FALSE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ("multipart/x-mixed-replace",
            image_resource->GetResponse().MimeType());

  const std::string_view kFirstPart =
      "--boundary\n"
      "Content-Type: image/svg+xml\n\n";
  image_resource->AppendData(kFirstPart);
  // Send the response for the first real part. No image or data buffer is
  // created.
  EXPECT_FALSE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ("image/svg+xml", image_resource->GetResponse().MimeType());

  const std::string_view kSecondPart =
      "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect "
      "width='1' height='1' fill='green'/></svg>\n";
  // The first bytes arrive. The data buffer is created, but no image is
  // created.
  image_resource->AppendData(kSecondPart);
  EXPECT_TRUE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());

  // Add an observer to check an assertion error doesn't happen
  // (crbug.com/630983).
  auto* observer2 = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());
  EXPECT_EQ(0, observer2->ImageChangedCount());
  EXPECT_FALSE(observer2->ImageNotifyFinishedCalled());

  const std::string_view kThirdPart = "--boundary";
  image_resource->AppendData(kThirdPart);
  ASSERT_TRUE(image_resource->ResourceBuffer());
  EXPECT_EQ(kSecondPart.size() - 1, image_resource->ResourceBuffer()->size());

  // This part finishes. The image is created, callbacks are sent, and the data
  // buffer is cleared.
  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
  EXPECT_TRUE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
  EXPECT_TRUE(IsA<SVGImage>(image_resource->GetContent()->GetImage()));
  EXPECT_TRUE(image_resource->GetContent()
                  ->GetImage()
                  ->PaintImageForCurrentFrame()
                  .is_multipart());

  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(1, observer2->ImageChangedCount());
  EXPECT_TRUE(observer2->ImageNotifyFinishedCalled());
}

TEST_F(ImageResourceTest, BitmapMultipartImage) {
  ResourceFetcher* fetcher = CreateFetcher();
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());
  ResourceRequest resource_request(test_url);
  resource_request.SetInspectorId(CreateUniqueIdentifier());
  resource_request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());
  resource_request.SetReferrerPolicy(
      ReferrerUtils::MojoReferrerPolicyResolveDefault(
          resource_request.GetReferrerPolicy()));
  resource_request.SetPriority(WebURLRequest::Priority::kLow);
  ImageResource* image_resource =
      ImageResource::Create(resource_request, nullptr /* world */);
  fetcher->StartLoad(image_resource);

  ResourceResponse multipart_response(NullURL());
  multipart_response.SetMimeType(AtomicString("multipart/x-mixed-replace"));
  multipart_response.SetHttpHeaderField(
      http_names::kContentType,
      AtomicString("multipart/x-mixed-replace; boundary=boundary"));
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(multipart_response),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);
  EXPECT_FALSE(image_resource->GetContent()->HasImage());

  const std::string_view kBoundary = "--boundary\n";
  const std::string_view kContentType = "Content-Type: image/jpeg\n\n";
  image_resource->AppendData(kBoundary);
  image_resource->AppendData(kContentType);
  image_resource->AppendData(base::as_chars(base::span(kJpegImage)));
  image_resource->AppendData(kBoundary);
  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
  EXPECT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_TRUE(image_resource->GetContent()
                  ->GetImage()
                  ->PaintImageForCurrentFrame()
                  .is_multipart());
}

TEST_F(ImageResourceTest, CancelOnRemoveObserver) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());
  task_runner->SetTime(1);

  // Emulate starting a real load.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  fetcher->StartLoad(image_resource);
  MemoryCache::Get()->Add(image_resource);

  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // The load should still be alive, but a timer should be started to cancel the
  // load inside removeClient().
  observer->RemoveAsObserver();
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_TRUE(MemoryCache::Get()->ResourceForURLForTesting(test_url));

  // Trigger the cancel timer, ensure the load was cancelled and the resource
  // was evicted from the cache.
  task_runner->RunUntilIdle();
  EXPECT_EQ(ResourceStatus::kLoadError, image_resource->GetStatus());
  EXPECT_FALSE(MemoryCache::Get()->ResourceForURLForTesting(test_url));
}

class MockFinishObserver : public ResourceFinishObserver {
 public:
  static MockFinishObserver* Create() {
    return MakeGarbageCollected<testing::StrictMock<MockFinishObserver>>();
  }
  MOCK_METHOD0(NotifyFinished, void());
  String DebugName() const override { return "MockFinishObserver"; }

  void Trace(Visitor* visitor) const override {
    blink::ResourceFinishObserver::Trace(visitor);
  }

 protected:
  MockFinishObserver() = default;
};

TEST_F(ImageResourceTest, CancelWithImageAndFinishObserver) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());

  // Emulate starting a real load.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  fetcher->StartLoad(image_resource);
  MemoryCache::Get()->Add(image_resource);

  Persistent<MockFinishObserver> finish_observer = MockFinishObserver::Create();
  image_resource->AddFinishObserver(finish_observer,
                                    fetcher->GetTaskRunner().get());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType(AtomicString("image/jpeg"));
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(base::as_chars(base::span(kJpegImage)));
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // This shouldn't crash. crbug.com/701723
  image_resource->Loader()->Cancel();

  EXPECT_EQ(ResourceStatus::kLoadError, image_resource->GetStatus());
  EXPECT_FALSE(MemoryCache::Get()->ResourceForURLForTesting(test_url));

  // ResourceFinishObserver is notified asynchronously.
  EXPECT_CALL(*finish_observer, NotifyFinished());
  task_runner->RunUntilIdle();
}

TEST_F(ImageResourceTest, DecodedDataRemainsWhileHasClients) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  image_resource->NotifyStartLoad();

  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType(AtomicString("multipart/x-mixed-replace"));
  image_resource->ResponseReceived(resource_response);

  resource_response.SetMimeType(AtomicString("image/jpeg"));
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(base::as_chars(base::span(kJpegImage)));
  image_resource->FinishForTest();
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());

  // The prune comes when the ImageResource still has observers. The image
  // should not be deleted.
  image_resource->Prune();
  EXPECT_TRUE(image_resource->IsAlive());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());

  // The ImageResource no longer has observers. The decoded image data should be
  // deleted by prune.
  observer->RemoveAsObserver();
  image_resource->Prune();
  EXPECT_FALSE(image_resource->IsAlive());
  EXPECT_TRUE(image_resource->GetContent()->HasImage());
  // TODO(hajimehoshi): Should check imageResource doesn't have decoded image
  // data.
}

TEST_F(ImageResourceTest, UpdateBitmapImages) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  image_resource->NotifyStartLoad();

  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  // Send the image response.

  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType(AtomicString("image/jpeg"));
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(base::as_chars(base::span(kJpegImage)));
  image_resource->FinishForTest();
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
}

TEST_F(ImageResourceTest, SVGImage) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
}

TEST_F(ImageResourceTest, SVGImageWithSubresource) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml",
                  kSvgImageWithSubresource);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));

  // At this point, image is (mostly) available but the loading is not yet
  // finished because of SVG's subresources, and thus ImageChanged() or
  // ImageNotifyFinished() are not called.
  EXPECT_EQ(ResourceStatus::kPending,
            image_resource->GetContent()->GetContentStatus());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(198, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(100, image_resource->GetContent()->GetImage()->height());

  // A new client added here shouldn't notified of finish.
  auto* observer2 = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());
  EXPECT_EQ(1, observer2->ImageChangedCount());
  EXPECT_FALSE(observer2->ImageNotifyFinishedCalled());

  // After asynchronous tasks are executed, the loading of SVG document is
  // completed and ImageNotifyFinished() is called.
  test::RunPendingTasks();
  EXPECT_EQ(ResourceStatus::kCached,
            image_resource->GetContent()->GetContentStatus());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(2, observer2->ImageChangedCount());
  EXPECT_TRUE(observer2->ImageNotifyFinishedCalled());
  EXPECT_EQ(198, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(100, image_resource->GetContent()->GetImage()->height());

  MemoryCache::Get()->EvictResources();
}

TEST_F(ImageResourceTest, SuccessfulRevalidationJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse resource_response(url);
  resource_response.SetHttpStatusCode(304);

  image_resource->ResponseReceived(resource_response);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, SuccessfulRevalidationSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse resource_response(url);
  resource_response.SetHttpStatusCode(304);
  image_resource->ResponseReceived(resource_response);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, FailedRevalidationJpegToJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage2)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(4, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, FailedRevalidationJpegToSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(3, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, FailedRevalidationSvgToJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(3, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, FailedRevalidationSvgToSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage2);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(300, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(300, image_resource->GetContent()->GetImage()->height());
}

// Tests for pruning.

TEST_F(ImageResourceTest, Prune) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);

  ReceiveResponse(image_resource, url, "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  EXPECT_FALSE(image_resource->IsAlive());

  image_resource->Prune();

  EXPECT_TRUE(image_resource->GetContent()->HasImage());

  blink::test::RunPendingTasks();
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, CancelOnDecodeError) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters params =
      FetchParameters::CreateForTest(ResourceRequest(test_url));
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ResourceResponse resource_response(test_url);
  resource_response.SetMimeType(AtomicString("image/jpeg"));
  resource_response.SetExpectedContentLength(18);
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);

  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidReceiveDataForTesting(
      base::span_from_cstring("notactuallyanimage"));

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

TEST_F(ImageResourceTest, DecodeErrorWithEmptyBody) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters params =
      FetchParameters::CreateForTest(ResourceRequest(test_url));
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ResourceResponse resource_response(test_url);
  resource_response.SetMimeType(AtomicString("image/jpeg"));
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);

  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

// Testing DecodeError that occurs in didFinishLoading().
// This is similar to DecodeErrorWithEmptyBody, but with non-empty body.
TEST_F(ImageResourceTest, PartialContentWithoutDimensions) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceRequest resource_request(test_url);
  resource_request.SetHttpHeaderField(http_names::kLowerRange,
                                      AtomicString("bytes=0-2"));
  FetchParameters params =
      FetchParameters::CreateForTest(std::move(resource_request));
  ResourceFetcher* fetcher = CreateFetcher();
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  ResourceResponse partial_response(test_url);
  partial_response.SetMimeType(AtomicString("image/jpeg"));
  partial_response.SetExpectedContentLength(
      kJpegImageSubrangeWithoutDimensionsLength);
  partial_response.SetHttpStatusCode(206);
  partial_response.SetHttpHeaderField(
      http_names::kLowerContentRange,
      BuildContentRange(kJpegImageSubrangeWithoutDimensionsLength,
                        sizeof(kJpegImage)));

  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(partial_response),
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);
  image_resource->Loader()->DidReceiveDataForTesting(
      base::make_span(reinterpret_cast<const char*>(kJpegImage),
                      kJpegImageSubrangeWithoutDimensionsLength));

  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidFinishLoading(
      base::TimeTicks(), kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

TEST_F(ImageResourceTest, PeriodicFlushTest) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  std::unique_ptr<DummyPageHolder> page_holder =
      std::make_unique<DummyPageHolder>(
          gfx::Size(800, 600), /*chrome_client=*/nullptr,
          MakeGarbageCollected<EmptyLocalFrameClient>());

  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      page_holder->GetFrame().GetTaskRunner(TaskType::kInternalTest);
  scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner =
      page_holder->GetFrame().GetTaskRunner(TaskType::kInternalTest);
  auto* context = MakeGarbageCollected<MockFetchContext>();
  auto& properties =
      MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties, context, task_runner, unfreezable_task_runner,
      MakeGarbageCollected<TestLoaderFactory>(),
      page_holder->GetFrame().DomWindow(),
      nullptr /* back_forward_cache_loader_helper */));
  auto frame_scheduler = std::make_unique<scheduler::FakeFrameScheduler>();
  auto* scheduler = MakeGarbageCollected<ResourceLoadScheduler>(
      ResourceLoadScheduler::ThrottlingPolicy::kNormal,
      ResourceLoadScheduler::ThrottleOptionOverride::kNone, properties,
      frame_scheduler.get(), *MakeGarbageCollected<DetachableConsoleLogger>(),
      /*loading_behavior_observer=*/nullptr);
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  // Ensure that |image_resource| has a loader.
  [[maybe_unused]] auto* loader = MakeGarbageCollected<ResourceLoader>(
      fetcher, scheduler, image_resource, page_holder->GetFrame().DomWindow());

  image_resource->NotifyStartLoad();

  auto* observer = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType(AtomicString("image/jpeg"));
  resource_response.SetExpectedContentLength(sizeof(kJpegImage2));
  image_resource->ResponseReceived(resource_response);

  // This is number is sufficiently large amount of bytes necessary for the
  // image to be created (since the size is known). This was determined by
  // appending one byte at a time (with flushes) until the image was decoded.
  size_t meaningful_image_size = 280;
  base::span<const char> remaining = base::as_chars(base::span(kJpegImage2));
  image_resource->AppendData(remaining.first(meaningful_image_size));
  remaining = remaining.subspan(meaningful_image_size);

  EXPECT_FALSE(image_resource->ErrorOccurred());
  EXPECT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(1, observer->ImageChangedCount());

  platform->RunForPeriodSeconds(1.);
  platform->AdvanceClockSeconds(1.);

  // Sanity check that we created an image after appending |meaningfulImageSize|
  // bytes just once.
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(1, observer->ImageChangedCount());

  for (int flush_count = 1; flush_count <= 3; ++flush_count) {
    // For each of the iteration that appends data, we don't expect
    // |imageChangeCount()| to change, since the time is adjusted by 0.2001
    // seconds (it's greater than 0.2 to avoid double precision problems).
    // After 5 appends, we breach the flush interval and the flush count
    // increases.
    for (int i = 0; i < 5; ++i) {
      SCOPED_TRACE(i);
      image_resource->AppendData(remaining.first(1u));
      remaining = remaining.subspan(1u);

      EXPECT_FALSE(image_resource->ErrorOccurred());
      ASSERT_TRUE(image_resource->GetContent()->HasImage());
      EXPECT_EQ(flush_count, observer->ImageChangedCount());

      platform->RunForPeriodSeconds(0.2001);
    }
  }

  // Increasing time by a large number only causes one extra flush.
  platform->RunForPeriodSeconds(10.);
  platform->AdvanceClockSeconds(10.);
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(4, observer->ImageChangedCount());

  // Append the rest of the data and finish (which causes another flush).
  image_resource->AppendData(remaining);
  image_resource->FinishForTest();

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(5, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(IsA<BitmapImage>(image_resource->GetContent()->GetImage()));
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->height());
}

TEST_F(ImageResourceTest, DeferredInvalidation) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  auto* obs = MakeGarbageCollected<MockImageResourceObserver>(
      image_resource->GetContent());

  // Image loaded.
  ReceiveResponse(image_resource, NullURL(), "image/jpeg",
                  base::as_chars(base::span(kJpegImage)));
  EXPECT_EQ(obs->ImageChangedCount(), 2);
  EXPECT_EQ(obs->Defer(), ImageResourceObserver::CanDeferInvalidation::kNo);

  // Image animated.
  static_cast<ImageObserver*>(image_resource->GetContent())
      ->Changed(image_resource->GetContent()->GetImage());
  EXPECT_EQ(obs->ImageChangedCount(), 3);
  EXPECT_EQ(obs->Defer(), ImageResourceObserver::CanDeferInvalidation::kYes);
}

// A lossy 2x2 WebP image.
constexpr unsigned char kLossyWebPImage[] = {
    0x52, 0x49, 0x46, 0x46, 0x40, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x20, 0x34, 0x00, 0x00, 0x00, 0xB0, 0x01, 0x00, 0x9D,
    0x01, 0x2A, 0x02, 0x00, 0x02, 0x00, 0x00, 0xC0, 0x12, 0x25, 0x00, 0x4E,
    0x80, 0x21, 0xDF, 0xC0, 0x5D, 0x80, 0x00, 0xFE, 0x9B, 0x87, 0xFA, 0x8F,
    0xF8, 0xA0, 0x1E, 0xD7, 0xC8, 0x70, 0x88, 0x0B, 0x6C, 0x54, 0x7F, 0xC0,
    0x7F, 0x12, 0xFE, 0xC0, 0xBC, 0x70, 0x65, 0xB6, 0xC1, 0x00, 0x00, 0x00};

// A lossless 2x2 WebP image.
constexpr unsigned char kLosslessWebPImage[] = {
    0x52, 0x49, 0x46, 0x46, 0x3A, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42,
    0x50, 0x56, 0x50, 0x38, 0x4C, 0x2E, 0x00, 0x00, 0x00, 0x2F, 0x01,
    0x40, 0x00, 0x00, 0x1F, 0x20, 0x10, 0x20, 0x72, 0xCC, 0x09, 0x13,
    0x27, 0x48, 0x40, 0x42, 0xB8, 0xE3, 0xB9, 0x95, 0x12, 0x12, 0x10,
    0x2B, 0x0C, 0xB4, 0xD8, 0x1C, 0x75, 0xFE, 0x03, 0xDB, 0xBA, 0x09,
    0x40, 0x26, 0x6D, 0x1B, 0x6A, 0xBB, 0x6B, 0x11, 0xFD, 0x8F, 0x1D};

// An extended lossy 2x2 WebP image.
constexpr unsigned char kExtendedWebPImage[] = {
    0x52, 0x49, 0x46, 0x46, 0x60, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x58, 0x0A, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x41, 0x4C, 0x50, 0x48, 0x05, 0x00,
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xE9, 0xAE, 0x00, 0x56, 0x50, 0x38, 0x20,
    0x34, 0x00, 0x00, 0x00, 0xB0, 0x01, 0x00, 0x9D, 0x01, 0x2A, 0x02, 0x00,
    0x02, 0x00, 0x00, 0xC0, 0x12, 0x25, 0x94, 0x02, 0x74, 0x01, 0x0E, 0xFE,
    0x02, 0xEC, 0x00, 0xFE, 0x9B, 0x87, 0xFA, 0x8F, 0xF8, 0xA0, 0x1E, 0xD7,
    0xC8, 0x70, 0x88, 0x0B, 0x6C, 0x54, 0x7A, 0xFB, 0xCA, 0x1D, 0x89, 0x90,
    0xDD, 0x27, 0xEA, 0x7F, 0x28, 0x00, 0x00, 0x00};

TEST_F(ImageResourceTest, WebPSniffing) {
  KURL test_url(kTestURL);

  // Test lossy WebP image.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(base::as_chars(base::span(kLossyWebPImage)));
  EXPECT_EQ(1, image_resource->GetContent()->GetCompressionFormat());

  // Test lossless WebP image.
  image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(base::as_chars(base::span(kLosslessWebPImage)));
  EXPECT_EQ(2, image_resource->GetContent()->GetCompressionFormat());

  // Test extended WebP image.
  image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(base::as_chars(base::span(kExtendedWebPImage)));
  EXPECT_EQ(1, image_resource->GetContent()->GetCompressionFormat());
}

}  // namespace

class ImageResourceCounterTest : public testing::Test {
 public:
  ImageResourceCounterTest() = default;
  ~ImageResourceCounterTest() override = default;

  void CreateImageResource(const char* url_part, bool ua_resource) {
    // Create a unique fake data url.
    String url = StringView("data:image/png;base64,") + url_part;

    // Setup the fetcher and request.
    ResourceFetcher* fetcher = CreateFetcher();
    KURL test_url(url);
    ResourceRequest request = ResourceRequest(test_url);
    FetchParameters fetch_params =
        FetchParameters::CreateForTest(std::move(request));
    scheduler::FakeTaskRunner* task_runner =
        static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());
    task_runner->SetTime(1);

    // Mark it as coming from a UA stylesheet (if needed).
    if (ua_resource) {
      fetch_params.MutableOptions().initiator_info.name =
          fetch_initiator_type_names::kUacss;
    }

    // Fetch the ImageResource.
    ImageResource::Fetch(fetch_params, fetcher);
    task_runner->RunUntilIdle();
  }

  int GetResourceCount() const {
    return InstanceCounters::CounterValue(InstanceCounters::kResourceCounter);
  }

  int GetUACSSResourceCount() const {
    return InstanceCounters::CounterValue(
        InstanceCounters::kUACSSResourceCounter);
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(ImageResourceCounterTest, InstanceCounters) {
  // Get the current resource count.
  int current_count = GetResourceCount();
  int current_ua_count = GetUACSSResourceCount();

  // Create a non-UA sourced image.
  CreateImageResource("a", false);

  // Check the instance counters have been updated.
  EXPECT_EQ(++current_count, GetResourceCount());
  EXPECT_EQ(current_ua_count, GetUACSSResourceCount());

  // Create another non-UA sourced image.
  CreateImageResource("b", false);

  // Check the instance counters have been updated.
  EXPECT_EQ(++current_count, GetResourceCount());
  EXPECT_EQ(current_ua_count, GetUACSSResourceCount());
}

TEST_F(ImageResourceCounterTest, InstanceCounters_UserAgent) {
  // Get the current resource count.
  int current_count = GetResourceCount();
  int current_ua_count = GetUACSSResourceCount();

  // Create a non-UA sourced image.
  CreateImageResource("c", false);

  // Check the instance counters have been updated.
  EXPECT_EQ(++current_count, GetResourceCount());
  EXPECT_EQ(current_ua_count, GetUACSSResourceCount());

  // Create a UA sourced image.
  CreateImageResource("d", true);

  // Check the instance counters have been updated.
  EXPECT_EQ(++current_count, GetResourceCount());
  EXPECT_EQ(++current_ua_count, GetUACSSResourceCount());
}

TEST_F(ImageResourceCounterTest, RevalidationPolicyMetrics) {
  base::HistogramTester histogram_tester;
  auto* fetcher = CreateFetcher();

  KURL test_url("http://127.0.0.1:8000/img.png");
  ScopedMockedURLLoad url_load(test_url, GetTestFilePath());

  // Test image preloads are immediately loaded.
  FetchParameters fetch_params =
      FetchParameters::CreateForTest(ResourceRequest(test_url));
  fetch_params.SetLinkPreload(true);

  Resource* resource = ImageResource::Fetch(fetch_params, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  Resource* new_resource = ImageResource::Fetch(fetch_params, fetcher);
  EXPECT_EQ(resource, new_resource);

  // Test histograms.
  histogram_tester.ExpectTotalCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Image", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Image",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kLoad),
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Preload.Image",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kUse), 1);

  KURL test_url_deferred("http://127.0.0.1:8000/img_deferred.ttf");
  ScopedMockedURLLoad url_load_deferred(test_url_deferred, GetTestFilePath());

  // Test deferred image loads are correctly counted.
  FetchParameters fetch_params_deferred =
      FetchParameters::CreateForTest(ResourceRequest(test_url_deferred));
  fetch_params_deferred.SetLazyImageDeferred();
  resource = ImageResource::Fetch(fetch_params_deferred, fetcher);
  ASSERT_TRUE(resource);
  histogram_tester.ExpectTotalCount(
      "Blink.MemoryCache.RevalidationPolicy.Image", 1);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Image",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kDefer),
      1);
  fetcher->StartLoad(resource);
  histogram_tester.ExpectTotalCount(
      "Blink.MemoryCache.RevalidationPolicy.Image", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Image",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::
                           kPreviouslyDeferredLoad),
      1);
  // Load the same deferred image again. Already-loaded resources shall be
  // counted as kUse.
  resource = ImageResource::Fetch(fetch_params_deferred, fetcher);
  histogram_tester.ExpectTotalCount(
      "Blink.MemoryCache.RevalidationPolicy.Image", 3);
  histogram_tester.ExpectBucketCount(
      "Blink.MemoryCache.RevalidationPolicy.Image",
      static_cast<int>(ResourceFetcher::RevalidationPolicyForMetrics::kUse), 1);
}

}  // namespace blink
