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

#include "third_party/blink/renderer/core/loader/resource/image_resource.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/mock_image_resource_observer.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
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

// Ensure that the image decoder can determine the dimensions of kJpegImage from
// just the first kJpegImageSubrangeWithDimensionsLength bytes. If this test
// fails, then the test data here probably needs to be updated.
TEST(ImageResourceTest, DimensionsDecodableFromPartialTestImage) {
  scoped_refptr<Image> image = BitmapImage::Create();
  EXPECT_EQ(
      Image::kSizeAvailable,
      image->SetData(SharedBuffer::Create(
                         kJpegImage, kJpegImageSubrangeWithDimensionsLength),
                     true));
  EXPECT_TRUE(image->IsBitmapImage());
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

constexpr char kSvgImage[] =
    "<svg width=\"200\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"100px\" height=\"100px\" fill=\"red\"/>"
    "</svg>";

constexpr char kSvgImage2[] =
    "<svg width=\"300\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"200px\" height=\"200px\" fill=\"green\"/>"
    "</svg>";

constexpr char kTestURL[] = "http://www.test.com/cancelTest.html";

String GetTestFilePath() {
  return test::CoreTestDataPath("cancelTest.html");
}

constexpr char kSvgImageWithSubresource[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"198\" height=\"100\">"
    "<style>"
    "  <![CDATA[@font-face{font-family:\"test\"; "
    "    src:url('data:font/ttf;base64,invalidFontData');}]]>"
    "</style>"
    "<text x=\"50\" y=\"50\" font-family=\"test\" font-size=\"16\">Fox</text>"
    "</svg>";

void ReceiveResponse(ImageResource* image_resource,
                     const KURL& url,
                     const AtomicString& mime_type,
                     const char* data,
                     size_t data_size) {
  ResourceResponse resource_response(url);
  resource_response.SetMimeType(mime_type);
  resource_response.SetHttpStatusCode(200);
  image_resource->NotifyStartLoad();
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(data, data_size);
  image_resource->FinishForTest();
}

void TestThatReloadIsStartedThenServeReload(
    const KURL& test_url,
    ImageResource* image_resource,
    ImageResourceContent* content,
    MockImageResourceObserver* observer,
    bool placeholder_before_reload) {
  const char* data = reinterpret_cast<const char*>(kJpegImage2);
  constexpr size_t kDataLength = sizeof(kJpegImage2);
  constexpr int kImageWidth = 50;
  constexpr int kImageHeight = 50;

  // Checks that |imageResource| and |content| are ready for non-placeholder
  // reloading.
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_FALSE(image_resource->ResourceBuffer());
  EXPECT_EQ(placeholder_before_reload, image_resource->ShouldShowPlaceholder());
  EXPECT_EQ(g_null_atom,
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_EQ(content, image_resource->GetContent());
  EXPECT_FALSE(content->HasImage());

  // Checks |observer| before reloading.
  const int original_image_changed_count = observer->ImageChangedCount();
  const bool already_notified_finish = observer->ImageNotifyFinishedCalled();
  const int image_width_on_image_notify_finished =
      observer->ImageWidthOnImageNotifyFinished();
  ASSERT_NE(kImageWidth, image_width_on_image_notify_finished);

  // Does Reload.
  ResourceResponse resource_response(test_url);
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(kDataLength);
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response));
  image_resource->Loader()->DidReceiveData(data, kDataLength);
  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), kDataLength,
                                             kDataLength, kDataLength, false);

  // Checks |imageResource|'s status after reloading.
  EXPECT_EQ(ResourceStatus::kCached, image_resource->GetStatus());
  EXPECT_FALSE(image_resource->ErrorOccurred());
  EXPECT_EQ(kDataLength, image_resource->EncodedSize());

  // Checks |observer| after reloading that it is notified of updates/finish.
  EXPECT_LT(original_image_changed_count, observer->ImageChangedCount());
  EXPECT_EQ(kImageWidth, observer->ImageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  if (!already_notified_finish) {
    // If imageNotifyFinished() has not been called before the reloaded
    // response is served, then imageNotifyFinished() should be called with
    // the new image (of width |imageWidth|).
    EXPECT_EQ(kImageWidth, observer->ImageWidthOnImageNotifyFinished());
  }

  // Checks |content| receives the correct image.
  EXPECT_TRUE(content->HasImage());
  EXPECT_FALSE(content->GetImage()->IsNull());
  EXPECT_EQ(kImageWidth, content->GetImage()->width());
  EXPECT_EQ(kImageHeight, content->GetImage()->height());
  EXPECT_FALSE(content->GetImage()->PaintImageForCurrentFrame().is_multipart());
}

AtomicString BuildContentRange(size_t range_length, size_t total_length) {
  return AtomicString(String("bytes 0-" + String::Number(range_length - 1) +
                             "/" + String::Number(total_length)));
}

void TestThatIsPlaceholderRequestAndServeResponse(
    const KURL& url,
    ImageResource* image_resource,
    MockImageResourceObserver* observer) {
  // Checks that |imageResource| is requesting for placeholder.
  EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
  EXPECT_EQ("bytes=0-2047",
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_EQ(0, observer->ImageChangedCount());

  // Serves partial response that is sufficient for creating a placeholder.
  ResourceResponse resource_response(url);
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(
      kJpegImageSubrangeWithDimensionsLength);
  resource_response.SetHttpStatusCode(206);
  resource_response.SetHttpHeaderField(
      "content-range", BuildContentRange(kJpegImageSubrangeWithDimensionsLength,
                                         sizeof(kJpegImage)));
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response));
  image_resource->Loader()->DidReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithDimensionsLength);
  image_resource->Loader()->DidFinishLoading(
      base::TimeTicks(), kJpegImageSubrangeWithDimensionsLength,
      kJpegImageSubrangeWithDimensionsLength,
      kJpegImageSubrangeWithDimensionsLength, false);

  // Checks that |imageResource| is successfully loaded, showing a placeholder.
  EXPECT_EQ(ResourceStatus::kCached, image_resource->GetStatus());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            image_resource->EncodedSize());

  EXPECT_LT(0, observer->ImageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnImageNotifyFinished());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  // A placeholder image.
  EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsSVGImage());
}

void TestThatIsNotPlaceholderRequestAndServeResponse(
    const KURL& url,
    ImageResource* image_resource,
    MockImageResourceObserver* observer) {
  // Checks that |imageResource| is NOT requesting for placeholder.
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());
  EXPECT_EQ(g_null_atom,
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_EQ(0, observer->ImageChangedCount());

  // Serves full response.
  ResourceResponse resource_response(url);
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response));
  image_resource->Loader()->DidReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
  image_resource->Loader()->DidFinishLoading(
      base::TimeTicks(), sizeof(kJpegImage), sizeof(kJpegImage),
      sizeof(kJpegImage), false);

  // Checks that |imageResource| is successfully loaded,
  // showing a non-placeholder image.
  EXPECT_EQ(ResourceStatus::kCached, image_resource->GetStatus());
  EXPECT_EQ(sizeof(kJpegImage), image_resource->EncodedSize());

  EXPECT_LT(0, observer->ImageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnImageNotifyFinished());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  // A non-placeholder bitmap image.
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsSVGImage());
}

ResourceFetcher* CreateFetcher() {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), MakeGarbageCollected<MockFetchContext>(),
      base::MakeRefCounted<scheduler::FakeTaskRunner>(),
      MakeGarbageCollected<TestLoaderFactory>()));
}

TEST(ImageResourceTest, MultipartImage) {
  ResourceFetcher* fetcher = CreateFetcher();
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  // Emulate starting a real load, but don't expect any "real"
  // WebURLLoaderClient callbacks.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);
  fetcher->StartLoad(image_resource);

  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // Send the multipart response. No image or data buffer is created. Note that
  // the response must be routed through ResourceLoader to ensure the load is
  // flagged as multipart.
  ResourceResponse multipart_response(NullURL());
  multipart_response.SetMimeType("multipart/x-mixed-replace");
  multipart_response.SetHttpHeaderField(
      http_names::kContentType, "multipart/x-mixed-replace; boundary=boundary");
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(multipart_response));
  EXPECT_FALSE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ("multipart/x-mixed-replace",
            image_resource->GetResponse().MimeType());

  const char kFirstPart[] =
      "--boundary\n"
      "Content-Type: image/svg+xml\n\n";
  image_resource->AppendData(kFirstPart, strlen(kFirstPart));
  // Send the response for the first real part. No image or data buffer is
  // created.
  EXPECT_FALSE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ("image/svg+xml", image_resource->GetResponse().MimeType());

  const char kSecondPart[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect "
      "width='1' height='1' fill='green'/></svg>\n";
  // The first bytes arrive. The data buffer is created, but no image is
  // created.
  image_resource->AppendData(kSecondPart, strlen(kSecondPart));
  EXPECT_TRUE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(0, observer->ImageChangedCount());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());

  // Add an observer to check an assertion error doesn't happen
  // (crbug.com/630983).
  auto observer2 =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());
  EXPECT_EQ(0, observer2->ImageChangedCount());
  EXPECT_FALSE(observer2->ImageNotifyFinishedCalled());

  const char kThirdPart[] = "--boundary";
  image_resource->AppendData(kThirdPart, strlen(kThirdPart));
  ASSERT_TRUE(image_resource->ResourceBuffer());
  EXPECT_EQ(strlen(kSecondPart) - 1, image_resource->ResourceBuffer()->size());

  // This part finishes. The image is created, callbacks are sent, and the data
  // buffer is cleared.
  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
  EXPECT_TRUE(image_resource->ResourceBuffer());
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsSVGImage());
  EXPECT_TRUE(image_resource->GetContent()
                  ->GetImage()
                  ->PaintImageForCurrentFrame()
                  .is_multipart());

  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(1, observer2->ImageChangedCount());
  EXPECT_TRUE(observer2->ImageNotifyFinishedCalled());
}

TEST(ImageResourceTest, BitmapMultipartImage) {
  ResourceFetcher* fetcher = CreateFetcher();
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());
  ResourceRequest resource_request(test_url);
  resource_request.SetInspectorId(CreateUniqueIdentifier());
  ImageResource* image_resource = ImageResource::Create(resource_request);
  fetcher->StartLoad(image_resource);

  ResourceResponse multipart_response(NullURL());
  multipart_response.SetMimeType("multipart/x-mixed-replace");
  multipart_response.SetHttpHeaderField(
      http_names::kContentType, "multipart/x-mixed-replace; boundary=boundary");
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(multipart_response));
  EXPECT_FALSE(image_resource->GetContent()->HasImage());

  const char kBoundary[] = "--boundary\n";
  const char kContentType[] = "Content-Type: image/jpeg\n\n";
  image_resource->AppendData(kBoundary, strlen(kBoundary));
  image_resource->AppendData(kContentType, strlen(kContentType));
  image_resource->AppendData(reinterpret_cast<const char*>(kJpegImage),
                             sizeof(kJpegImage));
  image_resource->AppendData(kBoundary, strlen(kBoundary));
  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
  EXPECT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_TRUE(image_resource->GetContent()
                  ->GetImage()
                  ->PaintImageForCurrentFrame()
                  .is_multipart());
}

TEST(ImageResourceTest, CancelOnRemoveObserver) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  scheduler::FakeTaskRunner* task_runner =
      static_cast<scheduler::FakeTaskRunner*>(fetcher->GetTaskRunner().get());
  task_runner->SetTime(1);

  // Emulate starting a real load.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  fetcher->StartLoad(image_resource);
  GetMemoryCache()->Add(image_resource);

  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // The load should still be alive, but a timer should be started to cancel the
  // load inside removeClient().
  observer->RemoveAsObserver();
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_TRUE(GetMemoryCache()->ResourceForURL(test_url));

  // Trigger the cancel timer, ensure the load was cancelled and the resource
  // was evicted from the cache.
  task_runner->RunUntilIdle();
  EXPECT_EQ(ResourceStatus::kLoadError, image_resource->GetStatus());
  EXPECT_FALSE(GetMemoryCache()->ResourceForURL(test_url));
}

class MockFinishObserver : public GarbageCollected<MockFinishObserver>,
                           public ResourceFinishObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MockFinishObserver);

 public:
  static MockFinishObserver* Create() {
    return

        MakeGarbageCollected<testing::StrictMock<MockFinishObserver>>();
  }
  MOCK_METHOD0(NotifyFinished, void());
  String DebugName() const override { return "MockFinishObserver"; }

  void Trace(blink::Visitor* visitor) override {
    blink::ResourceFinishObserver::Trace(visitor);
  }

 protected:
  MockFinishObserver() = default;
};

TEST(ImageResourceTest, CancelWithImageAndFinishObserver) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();

  // Emulate starting a real load.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  fetcher->StartLoad(image_resource);
  GetMemoryCache()->Add(image_resource);

  Persistent<MockFinishObserver> finish_observer = MockFinishObserver::Create();
  image_resource->AddFinishObserver(finish_observer,
                                    fetcher->GetTaskRunner().get());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(reinterpret_cast<const char*>(kJpegImage),
                             sizeof(kJpegImage));
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());

  // This shouldn't crash. crbug.com/701723
  image_resource->Loader()->Cancel();

  EXPECT_EQ(ResourceStatus::kLoadError, image_resource->GetStatus());
  EXPECT_FALSE(GetMemoryCache()->ResourceForURL(test_url));

  // ResourceFinishObserver is notified asynchronously.
  EXPECT_CALL(*finish_observer, NotifyFinished());
  blink::test::RunPendingTasks();
}

TEST(ImageResourceTest, DecodedDataRemainsWhileHasClients) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  image_resource->NotifyStartLoad();

  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType("multipart/x-mixed-replace");
  image_resource->ResponseReceived(resource_response);

  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(reinterpret_cast<const char*>(kJpegImage),
                             sizeof(kJpegImage));
  EXPECT_NE(0u, image_resource->EncodedSizeMemoryUsageForTesting());
  image_resource->FinishForTest();
  EXPECT_EQ(0u, image_resource->EncodedSizeMemoryUsageForTesting());
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

TEST(ImageResourceTest, UpdateBitmapImages) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  image_resource->NotifyStartLoad();

  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  // Send the image response.

  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(sizeof(kJpegImage));
  image_resource->ResponseReceived(resource_response);
  image_resource->AppendData(reinterpret_cast<const char*>(kJpegImage),
                             sizeof(kJpegImage));
  image_resource->FinishForTest();
  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
}

TEST(ImageResourceReloadTest, ReloadIfLoFiOrPlaceholderForPlaceholder) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters params{ResourceRequest(test_url)};
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params.GetImageRequestOptimization());
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  TestThatIsPlaceholderRequestAndServeResponse(test_url, image_resource,
                                               observer.get());

  image_resource->ReloadIfLoFiOrPlaceholderImage(fetcher,
                                                 Resource::kReloadAlways);

  TestThatReloadIsStartedThenServeReload(test_url, image_resource,
                                         image_resource->GetContent(),
                                         observer.get(), false);
}

TEST(ImageResourceTest, SVGImage) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
}

TEST(ImageResourceTest, SVGImageWithSubresource) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml",
                  kSvgImageWithSubresource, strlen(kSvgImageWithSubresource));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());

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
  auto observer2 =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());
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

  GetMemoryCache()->EvictResources();
}

TEST(ImageResourceTest, SuccessfulRevalidationJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
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
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, SuccessfulRevalidationSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
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
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage2),
                  sizeof(kJpegImage2));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(4, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(3, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToJpeg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(3, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(kJpegImageWidth, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(kJpegImageHeight,
            image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToSvg) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(200, image_resource->GetContent()->GetImage()->height());

  image_resource->SetRevalidatingRequest(ResourceRequest(url));
  ReceiveResponse(image_resource, url, "image/svg+xml", kSvgImage2,
                  strlen(kSvgImage2));

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(300, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(300, image_resource->GetContent()->GetImage()->height());
}

// Tests for pruning.

TEST(ImageResourceTest, Prune) {
  KURL url("http://127.0.0.1:8000/foo");
  ImageResource* image_resource = ImageResource::CreateForTest(url);

  ReceiveResponse(image_resource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

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

TEST(ImageResourceTest, CancelOnDecodeError) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters params{ResourceRequest(test_url)};
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ResourceResponse resource_response(test_url);
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(18);
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response));

  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidReceiveData("notactuallyanimage", 18);

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

TEST(ImageResourceTest, DecodeErrorWithEmptyBody) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters params{ResourceRequest(test_url)};
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ResourceResponse resource_response(test_url);
  resource_response.SetMimeType("image/jpeg");
  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(resource_response));

  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

// Testing DecodeError that occurs in didFinishLoading().
// This is similar to DecodeErrorWithEmptyBody, but with non-empty body.
TEST(ImageResourceTest, PartialContentWithoutDimensions) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceRequest resource_request(test_url);
  resource_request.SetHttpHeaderField("range", "bytes=0-2");
  FetchParameters params(resource_request);
  ResourceFetcher* fetcher = CreateFetcher();
  ImageResource* image_resource = ImageResource::Fetch(params, fetcher);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  ResourceResponse partial_response(test_url);
  partial_response.SetMimeType("image/jpeg");
  partial_response.SetExpectedContentLength(
      kJpegImageSubrangeWithoutDimensionsLength);
  partial_response.SetHttpStatusCode(206);
  partial_response.SetHttpHeaderField(
      "content-range",
      BuildContentRange(kJpegImageSubrangeWithoutDimensionsLength,
                        sizeof(kJpegImage)));

  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(partial_response));
  image_resource->Loader()->DidReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_EQ(ResourceStatus::kPending, image_resource->GetStatus());
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidFinishLoading(
      base::TimeTicks(), kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength, false);

  EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            observer->StatusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->IsLoading());
}

TEST(ImageResourceTest, FetchDisallowPlaceholder) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  FetchParameters params{ResourceRequest(test_url)};
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kNone, params.GetImageRequestOptimization());
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  TestThatIsNotPlaceholderRequestAndServeResponse(test_url, image_resource,
                                                  observer.get());
}

TEST(ImageResourceTest, FetchAllowPlaceholderDataURL) {
  KURL test_url("data:image/jpeg;base64," + Base64Encode(kJpegImage));
  FetchParameters params{ResourceRequest(test_url)};
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kNone, params.GetImageRequestOptimization());
  EXPECT_EQ(g_null_atom,
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());
}

TEST(ImageResourceTest, FetchAllowPlaceholderPostRequest) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());
  ResourceRequest resource_request(test_url);
  resource_request.SetHttpMethod(http_names::kPOST);
  FetchParameters params(resource_request);
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kNone, params.GetImageRequestOptimization());
  EXPECT_EQ(g_null_atom,
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());

  image_resource->Loader()->Cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderExistingRangeHeader) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());
  ResourceRequest resource_request(test_url);
  resource_request.SetHttpHeaderField("range", "bytes=128-255");
  FetchParameters params(resource_request);
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kNone, params.GetImageRequestOptimization());
  EXPECT_EQ("bytes=128-255",
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());

  image_resource->Loader()->Cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderSuccessful) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  FetchParameters params{ResourceRequest(test_url)};
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params.GetImageRequestOptimization());
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  TestThatIsPlaceholderRequestAndServeResponse(test_url, image_resource,
                                               observer.get());
}

TEST(ImageResourceTest, FetchAllowPlaceholderUnsuccessful) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  FetchParameters params{ResourceRequest(test_url)};
  params.SetAllowImagePlaceholder();
  ImageResource* image_resource = ImageResource::Fetch(params, CreateFetcher());
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params.GetImageRequestOptimization());
  EXPECT_EQ("bytes=0-2047",
            image_resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  const char kBadData[] = "notanimageresponse";

  ResourceResponse bad_response(test_url);
  bad_response.SetMimeType("image/jpeg");
  bad_response.SetExpectedContentLength(sizeof(kBadData));
  bad_response.SetHttpStatusCode(206);
  bad_response.SetHttpHeaderField(
      "content-range", BuildContentRange(sizeof(kBadData), sizeof(kJpegImage)));

  image_resource->Loader()->DidReceiveResponse(
      WrappedResourceResponse(bad_response));

  EXPECT_EQ(0, observer->ImageChangedCount());

  image_resource->Loader()->DidReceiveData(kBadData, sizeof(kBadData));

  // The dimensions could not be extracted, so the full original image should be
  // loading.
  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_EQ(2, observer->ImageChangedCount());
  EXPECT_FALSE(image_resource->ShouldShowPlaceholder());

  TestThatReloadIsStartedThenServeReload(test_url, image_resource,
                                         image_resource->GetContent(),
                                         observer.get(), false);
}

TEST(ImageResourceTest, FetchAllowPlaceholderPartialContentWithoutDimensions) {
  const struct {
    WebURLRequest::PreviewsState initial_previews_state;
    WebURLRequest::PreviewsState expected_reload_previews_state;
    bool placeholder_before_reload;
    bool placeholder_after_reload;
  } tests[] = {
      {WebURLRequest::kPreviewsUnspecified, WebURLRequest::kPreviewsNoTransform,
       false},
  };

  for (const auto& test : tests) {
    KURL test_url(kTestURL);
    ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

    ResourceRequest resource_request(test_url);
    resource_request.SetPreviewsState(test.initial_previews_state);
    FetchParameters params(resource_request);

    params.SetAllowImagePlaceholder();
    ImageResource* image_resource =
        ImageResource::Fetch(params, CreateFetcher());
    EXPECT_EQ(FetchParameters::kAllowPlaceholder,
              params.GetImageRequestOptimization());
    EXPECT_EQ("bytes=0-2047",
              image_resource->GetResourceRequest().HttpHeaderField("range"));
    EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
    auto observer = std::make_unique<MockImageResourceObserver>(
        image_resource->GetContent());

    // TODO(hiroshige): Make the range request header and partial content length
    // consistent. https://crbug.com/689760.
    ResourceResponse partial_response(test_url);
    partial_response.SetMimeType("image/jpeg");
    partial_response.SetExpectedContentLength(
        kJpegImageSubrangeWithoutDimensionsLength);
    partial_response.SetHttpStatusCode(206);
    partial_response.SetHttpHeaderField(
        "content-range",
        BuildContentRange(kJpegImageSubrangeWithoutDimensionsLength,
                          sizeof(kJpegImage)));

    image_resource->Loader()->DidReceiveResponse(
        WrappedResourceResponse(partial_response));
    image_resource->Loader()->DidReceiveData(
        reinterpret_cast<const char*>(kJpegImage),
        kJpegImageSubrangeWithoutDimensionsLength);

    EXPECT_EQ(0, observer->ImageChangedCount());

    image_resource->Loader()->DidFinishLoading(
        base::TimeTicks(), kJpegImageSubrangeWithoutDimensionsLength,
        kJpegImageSubrangeWithoutDimensionsLength,
        kJpegImageSubrangeWithoutDimensionsLength, false);

    EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
    EXPECT_EQ(2, observer->ImageChangedCount());

    TestThatReloadIsStartedThenServeReload(
        test_url, image_resource, image_resource->GetContent(), observer.get(),
        test.placeholder_before_reload);

    EXPECT_EQ(test.expected_reload_previews_state,
              image_resource->GetResourceRequest().GetPreviewsState());
  }
}

TEST(ImageResourceTest, FetchAllowPlaceholderThenDisallowPlaceholder) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();

  FetchParameters placeholder_params{ResourceRequest(test_url)};
  placeholder_params.SetAllowImagePlaceholder();
  ImageResource* image_resource =
      ImageResource::Fetch(placeholder_params, fetcher);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  FetchParameters non_placeholder_params{ResourceRequest(test_url)};
  ImageResource* image_resource2 =
      ImageResource::Fetch(non_placeholder_params, fetcher);
  auto observer2 = std::make_unique<MockImageResourceObserver>(
      image_resource2->GetContent());

  ImageResource* image_resource3 =
      ImageResource::Fetch(non_placeholder_params, fetcher);
  auto observer3 = std::make_unique<MockImageResourceObserver>(
      image_resource3->GetContent());

  // |imageResource| remains a placeholder, while following non-placeholder
  // requests start non-placeholder loading with a separate ImageResource.
  ASSERT_NE(image_resource, image_resource2);
  ASSERT_NE(image_resource->Loader(), image_resource2->Loader());
  ASSERT_NE(image_resource->GetContent(), image_resource2->GetContent());
  ASSERT_EQ(image_resource2, image_resource3);

  EXPECT_FALSE(observer->ImageNotifyFinishedCalled());
  EXPECT_FALSE(observer2->ImageNotifyFinishedCalled());
  EXPECT_FALSE(observer3->ImageNotifyFinishedCalled());

  // Checks that |imageResource2| (and |imageResource3|) loads a
  // non-placeholder image.
  TestThatIsNotPlaceholderRequestAndServeResponse(test_url, image_resource2,
                                                  observer2.get());
  EXPECT_TRUE(observer3->ImageNotifyFinishedCalled());

  // Checks that |imageResource| will loads a placeholder image.
  TestThatIsPlaceholderRequestAndServeResponse(test_url, image_resource,
                                               observer.get());

  // |imageResource2| is still a non-placeholder image.
  EXPECT_FALSE(image_resource2->ShouldShowPlaceholder());
  EXPECT_TRUE(image_resource2->GetContent()->GetImage()->IsBitmapImage());
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderThenDisallowPlaceholderAfterLoaded) {
  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  ResourceFetcher* fetcher = CreateFetcher();
  FetchParameters placeholder_params{ResourceRequest(test_url)};
  placeholder_params.SetAllowImagePlaceholder();
  ImageResource* image_resource =
      ImageResource::Fetch(placeholder_params, fetcher);
  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  TestThatIsPlaceholderRequestAndServeResponse(test_url, image_resource,
                                               observer.get());

  FetchParameters non_placeholder_params{ResourceRequest(test_url)};
  ImageResource* image_resource2 =
      ImageResource::Fetch(non_placeholder_params, fetcher);
  auto observer2 = std::make_unique<MockImageResourceObserver>(
      image_resource2->GetContent());

  ImageResource* image_resource3 =
      ImageResource::Fetch(non_placeholder_params, fetcher);
  auto observer3 = std::make_unique<MockImageResourceObserver>(
      image_resource3->GetContent());

  EXPECT_FALSE(observer2->ImageNotifyFinishedCalled());
  EXPECT_FALSE(observer3->ImageNotifyFinishedCalled());

  // |imageResource| remains a placeholder, while following non-placeholder
  // requests start non-placeholder loading with a separate ImageResource.
  ASSERT_NE(image_resource, image_resource2);
  ASSERT_EQ(image_resource2, image_resource3);

  TestThatIsNotPlaceholderRequestAndServeResponse(test_url, image_resource2,
                                                  observer2.get());
  EXPECT_TRUE(observer3->ImageNotifyFinishedCalled());
}

TEST(ImageResourceTest, FetchAllowPlaceholderFullResponseDecodeSuccess) {
  const struct {
    int status_code;
    AtomicString content_range;
  } tests[] = {
      {200, g_null_atom},
      {404, g_null_atom},
      {206, BuildContentRange(sizeof(kJpegImage), sizeof(kJpegImage))},
  };
  for (const auto& test : tests) {
    KURL test_url(kTestURL);
    ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

    FetchParameters params{ResourceRequest(test_url)};
    params.SetAllowImagePlaceholder();
    ImageResource* image_resource =
        ImageResource::Fetch(params, CreateFetcher());
    EXPECT_EQ(FetchParameters::kAllowPlaceholder,
              params.GetImageRequestOptimization());
    EXPECT_EQ("bytes=0-2047",
              image_resource->GetResourceRequest().HttpHeaderField("range"));
    EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
    auto observer = std::make_unique<MockImageResourceObserver>(
        image_resource->GetContent());

    ResourceResponse resource_response(test_url);
    resource_response.SetMimeType("imapge/jpeg");
    resource_response.SetExpectedContentLength(sizeof(kJpegImage));
    resource_response.SetHttpStatusCode(test.status_code);
    if (test.content_range != g_null_atom)
      resource_response.SetHttpHeaderField("content-range", test.content_range);
    image_resource->Loader()->DidReceiveResponse(
        WrappedResourceResponse(resource_response));
    image_resource->Loader()->DidReceiveData(
        reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
    image_resource->Loader()->DidFinishLoading(
        base::TimeTicks(), sizeof(kJpegImage), sizeof(kJpegImage),
        sizeof(kJpegImage), false);

    EXPECT_EQ(ResourceStatus::kCached, image_resource->GetStatus());
    EXPECT_EQ(sizeof(kJpegImage), image_resource->EncodedSize());
    EXPECT_FALSE(image_resource->ShouldShowPlaceholder());
    EXPECT_LT(0, observer->ImageChangedCount());
    EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnLastImageChanged());
    EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
    EXPECT_EQ(kJpegImageWidth, observer->ImageWidthOnImageNotifyFinished());

    ASSERT_TRUE(image_resource->GetContent()->HasImage());
    EXPECT_EQ(kJpegImageWidth,
              image_resource->GetContent()->GetImage()->width());
    EXPECT_EQ(kJpegImageHeight,
              image_resource->GetContent()->GetImage()->height());
    EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  }
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderFullResponseDecodeFailureNoReload) {
  static const char kBadImageData[] = "bad image data";

  const struct {
    int status_code;
    AtomicString content_range;
    uint32_t data_size;
  } tests[] = {
      {200, g_null_atom, sizeof(kBadImageData)},
      {206, BuildContentRange(sizeof(kBadImageData), sizeof(kBadImageData)),
       sizeof(kBadImageData)},
      {204, g_null_atom, 0},
  };
  for (const auto& test : tests) {
    KURL test_url(kTestURL);
    ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

    FetchParameters params{ResourceRequest(test_url)};
    params.SetAllowImagePlaceholder();
    ImageResource* image_resource =
        ImageResource::Fetch(params, CreateFetcher());
    EXPECT_EQ(FetchParameters::kAllowPlaceholder,
              params.GetImageRequestOptimization());
    EXPECT_EQ("bytes=0-2047",
              image_resource->GetResourceRequest().HttpHeaderField("range"));
    EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
    auto observer = std::make_unique<MockImageResourceObserver>(
        image_resource->GetContent());

    ResourceResponse resource_response(test_url);
    resource_response.SetMimeType("image/jpeg");
    resource_response.SetExpectedContentLength(test.data_size);
    resource_response.SetHttpStatusCode(test.status_code);
    if (test.content_range != g_null_atom)
      resource_response.SetHttpHeaderField("content-range", test.content_range);
    image_resource->Loader()->DidReceiveResponse(
        WrappedResourceResponse(resource_response));
    image_resource->Loader()->DidReceiveData(kBadImageData, test.data_size);

    EXPECT_EQ(ResourceStatus::kDecodeError, image_resource->GetStatus());
    EXPECT_FALSE(image_resource->ShouldShowPlaceholder());
  }
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderFullResponseDecodeFailureWithReload) {
  const int kStatusCodes[] = {404, 500};
  for (int status_code : kStatusCodes) {
    KURL test_url(kTestURL);
    ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

    FetchParameters params{ResourceRequest(test_url)};
    params.SetAllowImagePlaceholder();
    ImageResource* image_resource =
        ImageResource::Fetch(params, CreateFetcher());
    EXPECT_EQ(FetchParameters::kAllowPlaceholder,
              params.GetImageRequestOptimization());
    EXPECT_EQ("bytes=0-2047",
              image_resource->GetResourceRequest().HttpHeaderField("range"));
    EXPECT_TRUE(image_resource->ShouldShowPlaceholder());
    auto observer = std::make_unique<MockImageResourceObserver>(
        image_resource->GetContent());

    static const char kBadImageData[] = "bad image data";

    ResourceResponse resource_response(test_url);
    resource_response.SetMimeType("image/jpeg");
    resource_response.SetExpectedContentLength(sizeof(kBadImageData));
    resource_response.SetHttpStatusCode(status_code);
    image_resource->Loader()->DidReceiveResponse(
        WrappedResourceResponse(resource_response));
    image_resource->Loader()->DidReceiveData(kBadImageData,
                                             sizeof(kBadImageData));

    EXPECT_FALSE(observer->ImageNotifyFinishedCalled());

    // The dimensions could not be extracted, and the response code was a 4xx
    // error, so the full original image should be loading.
    TestThatReloadIsStartedThenServeReload(test_url, image_resource,
                                           image_resource->GetContent(),
                                           observer.get(), false);
  }
}

TEST(ImageResourceTest, PeriodicFlushTest) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  EmptyChromeClient* chrome_client = MakeGarbageCollected<EmptyChromeClient>();
  Page::PageClients clients;
  FillWithEmptyClients(clients);
  clients.chrome_client = chrome_client;
  std::unique_ptr<DummyPageHolder> page_holder =
      std::make_unique<DummyPageHolder>(
          IntSize(800, 600), &clients,
          MakeGarbageCollected<EmptyLocalFrameClient>());

  KURL test_url(kTestURL);
  ScopedMockedURLLoad scoped_mocked_url_load(test_url, GetTestFilePath());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      page_holder->GetFrame().GetTaskRunner(TaskType::kInternalTest);
  auto* context = MakeGarbageCollected<MockFetchContext>();
  auto& properties =
      MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(
      ResourceFetcherInit(properties, context, task_runner,
                          MakeGarbageCollected<TestLoaderFactory>()));
  auto frame_scheduler = std::make_unique<scheduler::FakeFrameScheduler>();
  auto* scheduler = MakeGarbageCollected<ResourceLoadScheduler>(
      ResourceLoadScheduler::ThrottlingPolicy::kNormal, properties,
      frame_scheduler.get(), *MakeGarbageCollected<DetachableConsoleLogger>());
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);

  // Ensure that |image_resource| has a loader.
  auto* loader =
      MakeGarbageCollected<ResourceLoader>(fetcher, scheduler, image_resource);
  ALLOW_UNUSED_LOCAL(loader);

  image_resource->NotifyStartLoad();

  auto observer =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  // Send the image response.
  ResourceResponse resource_response(NullURL());
  resource_response.SetMimeType("image/jpeg");
  resource_response.SetExpectedContentLength(sizeof(kJpegImage2));
  image_resource->ResponseReceived(resource_response);

  // This is number is sufficiently large amount of bytes necessary for the
  // image to be created (since the size is known). This was determined by
  // appending one byte at a time (with flushes) until the image was decoded.
  size_t meaningful_image_size = 280;
  image_resource->AppendData(reinterpret_cast<const char*>(kJpegImage2),
                             meaningful_image_size);
  size_t bytes_sent = meaningful_image_size;

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
      image_resource->AppendData(
          reinterpret_cast<const char*>(kJpegImage2) + bytes_sent, 1);

      EXPECT_FALSE(image_resource->ErrorOccurred());
      ASSERT_TRUE(image_resource->GetContent()->HasImage());
      EXPECT_EQ(flush_count, observer->ImageChangedCount());

      ++bytes_sent;
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
  image_resource->AppendData(
      reinterpret_cast<const char*>(kJpegImage2) + bytes_sent,
      sizeof(kJpegImage2) - bytes_sent);
  image_resource->FinishForTest();

  EXPECT_FALSE(image_resource->ErrorOccurred());
  ASSERT_TRUE(image_resource->GetContent()->HasImage());
  EXPECT_FALSE(image_resource->GetContent()->GetImage()->IsNull());
  EXPECT_EQ(5, observer->ImageChangedCount());
  EXPECT_TRUE(observer->ImageNotifyFinishedCalled());
  EXPECT_TRUE(image_resource->GetContent()->GetImage()->IsBitmapImage());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->width());
  EXPECT_EQ(50, image_resource->GetContent()->GetImage()->height());
}

TEST(ImageResourceTest, DeferredInvalidation) {
  ImageResource* image_resource = ImageResource::CreateForTest(NullURL());
  std::unique_ptr<MockImageResourceObserver> obs =
      std::make_unique<MockImageResourceObserver>(image_resource->GetContent());

  // Image loaded.
  ReceiveResponse(image_resource, NullURL(), "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));
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

TEST(ImageResourceTest, WebPSniffing) {
  KURL test_url(kTestURL);

  // Test lossy WebP image.
  ImageResource* image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(reinterpret_cast<const char*>(kLossyWebPImage),
                             sizeof(kLossyWebPImage));
  EXPECT_EQ(1, image_resource->GetContent()->GetCompressionFormat());

  // Test lossless WebP image.
  image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(reinterpret_cast<const char*>(kLosslessWebPImage),
                             sizeof(kLosslessWebPImage));
  EXPECT_EQ(2, image_resource->GetContent()->GetCompressionFormat());

  // Test extended WebP image.
  image_resource = ImageResource::CreateForTest(test_url);
  image_resource->AppendData(reinterpret_cast<const char*>(kExtendedWebPImage),
                             sizeof(kExtendedWebPImage));
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
    FetchParameters fetch_params(request);
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

}  // namespace blink
