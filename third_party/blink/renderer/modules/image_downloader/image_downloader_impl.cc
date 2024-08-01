// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/image_downloader/image_downloader_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/image_downloader/multi_resolution_image_resource_fetcher.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace {

WTF::Vector<SkBitmap> DecodeImageData(const std::string& data,
                                      const std::string& mime_type,
                                      const gfx::Size& preferred_size) {
  // Decode the image using Blink's image decoder.
  blink::WebData buffer(data.data(), data.size());
  WTF::Vector<SkBitmap> bitmaps;
  if (mime_type == "image/svg+xml") {
    SkBitmap bitmap = blink::WebImage::DecodeSVG(buffer, preferred_size);
    if (!bitmap.drawsNothing()) {
      bitmaps.push_back(bitmap);
    }
  } else {
    blink::WebVector<SkBitmap> original_bitmaps =
        blink::WebImage::FramesFromData(buffer);
    bitmaps.AppendRange(std::make_move_iterator(original_bitmaps.begin()),
                        std::make_move_iterator(original_bitmaps.end()));
    bitmaps.Reverse();
  }
  return bitmaps;
}

// Decodes a data: URL into one or more images, or no images in case of failure.
WTF::Vector<SkBitmap> ImagesFromDataUrl(const blink::KURL& url,
                                        const gfx::Size& preferred_size) {
  std::string mime_type, data;
  if (!blink::network_utils::IsDataURLMimeTypeSupported(url, &data,
                                                        &mime_type) ||
      data.empty()) {
    return WTF::Vector<SkBitmap>();
  }
  return DecodeImageData(data, mime_type, preferred_size);
}

//  Proportionally resizes the |image| to fit in a box of size
// |max_image_size|.
SkBitmap ResizeImage(const SkBitmap& image, uint32_t max_image_size) {
  if (max_image_size == 0) {
    return image;
  }
  uint32_t max_dimension = std::max(image.width(), image.height());
  if (max_dimension <= max_image_size) {
    return image;
  }
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST,
      static_cast<uint32_t>(image.width()) * max_image_size / max_dimension,
      static_cast<uint32_t>(image.height()) * max_image_size / max_dimension);
}

// Filters the array of bitmaps, removing all images that do not fit in a box of
// size |max_image_size|. Returns the result if it is not empty. Otherwise,
// find the smallest image in the array and resize it proportionally to fit
// in a box of size |max_image_size|.
// Sets |original_image_sizes| to the sizes of |images| before resizing. Both
// output vectors are guaranteed to have the same size.
void FilterAndResizeImagesForMaximalSize(
    const WTF::Vector<SkBitmap>& unfiltered,
    uint32_t max_image_size,
    WTF::Vector<SkBitmap>* images,
    WTF::Vector<gfx::Size>* original_image_sizes) {
  images->clear();
  original_image_sizes->clear();

  if (unfiltered.empty()) {
    return;
  }

  if (max_image_size == 0) {
    max_image_size = std::numeric_limits<uint32_t>::max();
  }

  const SkBitmap* min_image = nullptr;
  uint32_t min_image_size = std::numeric_limits<uint32_t>::max();
  // Filter the images by |max_image_size|, and also identify the smallest image
  // in case all the images are bigger than |max_image_size|.
  for (const SkBitmap& image : unfiltered) {
    uint32_t current_size = std::max(image.width(), image.height());
    if (current_size < min_image_size) {
      min_image = &image;
      min_image_size = current_size;
    }
    if (static_cast<uint32_t>(image.width()) <= max_image_size &&
        static_cast<uint32_t>(image.height()) <= max_image_size) {
      images->push_back(image);
      original_image_sizes->push_back(gfx::Size(image.width(), image.height()));
    }
  }
  DCHECK(min_image);
  if (images->size()) {
    return;
  }
  // Proportionally resize the minimal image to fit in a box of size
  // |max_image_size|.
  SkBitmap resized = ResizeImage(*min_image, max_image_size);
  // Drop null or empty SkBitmap.
  if (resized.drawsNothing()) {
    return;
  }
  images->push_back(resized);
  original_image_sizes->push_back(
      gfx::Size(min_image->width(), min_image->height()));
}

}  // namespace

namespace blink {

// static
const char ImageDownloaderImpl::kSupplementName[] = "ImageDownloader";

// static
ImageDownloaderImpl* ImageDownloaderImpl::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<ImageDownloaderImpl>(frame);
}

// static
void ImageDownloaderImpl::ProvideTo(LocalFrame& frame) {
  if (ImageDownloaderImpl::From(frame)) {
    return;
  }

  Supplement<LocalFrame>::ProvideTo(
      frame, MakeGarbageCollected<ImageDownloaderImpl>(frame));
}

ImageDownloaderImpl::ImageDownloaderImpl(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ExecutionContextLifecycleObserver(frame.DomWindow()),
      receiver_(this, frame.DomWindow()) {
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &ImageDownloaderImpl::CreateMojoService, WrapWeakPersistent(this)));
}

ImageDownloaderImpl::~ImageDownloaderImpl() {}

void ImageDownloaderImpl::CreateMojoService(
    mojo::PendingReceiver<mojom::blink::ImageDownloader> receiver) {
  receiver_.Bind(std::move(receiver),
                 GetSupplementable()->GetTaskRunner(TaskType::kNetworking));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&ImageDownloaderImpl::Dispose, WrapWeakPersistent(this)));
}

// ImageDownloader methods:
void ImageDownloaderImpl::DownloadImage(const KURL& image_url,
                                        bool is_favicon,
                                        const gfx::Size& preferred_size,
                                        uint32_t max_bitmap_size,
                                        bool bypass_cache,
                                        DownloadImageCallback callback) {
  // Constrain the preferred size by the max bitmap size. This will prevent
  // resizing of the resulting image if the preferred size is used.
  gfx::Size constrained_preferred_size(preferred_size);
  uint32_t max_preferred_dimension =
      std::max(preferred_size.width(), preferred_size.height());
  if (max_bitmap_size && max_bitmap_size < max_preferred_dimension) {
    float scale = float(max_bitmap_size) / max_preferred_dimension;
    constrained_preferred_size = gfx::ScaleToFlooredSize(preferred_size, scale);
  }
  auto download_callback =
      WTF::BindOnce(&ImageDownloaderImpl::DidDownloadImage,
                    WrapPersistent(this), max_bitmap_size, std::move(callback));

  if (!image_url.ProtocolIsData()) {
    FetchImage(image_url, is_favicon, constrained_preferred_size, bypass_cache,
               std::move(download_callback));
    // Will complete asynchronously via ImageDownloaderImpl::DidFetchImage.
    return;
  }

  WTF::Vector<SkBitmap> result_images =
      ImagesFromDataUrl(image_url, constrained_preferred_size);
  std::move(download_callback).Run(0, result_images);
}

void ImageDownloaderImpl::DownloadImageFromAxNode(
    int ax_node_id,
    const gfx::Size& preferred_size,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    DownloadImageCallback callback) {
  LocalFrame* frame = GetSupplementable();
  CHECK(frame);
  auto* document = frame->GetDocument();
  CHECK(document);
  auto* cache = document->ExistingAXObjectCache();

  const int NOT_FOUND = 404;

  // If accessibility is not enabled just return not found for the images.
  if (!cache) {
    std::move(callback).Run(NOT_FOUND, {}, {});
  }

  auto* obj = cache->ObjectFromAXID(ax_node_id);

  // Similarly if the object that the node id is referring to is not there, also
  // return not found.
  if (!obj) {
    std::move(callback).Run(NOT_FOUND, {}, {});
    return;
  }

  // Use the data url since the src attribute may not contain the scheme.
  KURL url(obj->ImageDataUrl(gfx::Size()));
  DownloadImage(url, /*is_favicon=*/false, preferred_size, max_bitmap_size,
                bypass_cache, std::move(callback));
}

void ImageDownloaderImpl::DidDownloadImage(
    uint32_t max_image_size,
    DownloadImageCallback callback,
    int32_t http_status_code,
    const WTF::Vector<SkBitmap>& images) {
  WTF::Vector<SkBitmap> result_images;
  WTF::Vector<gfx::Size> result_original_image_sizes;
  FilterAndResizeImagesForMaximalSize(images, max_image_size, &result_images,
                                      &result_original_image_sizes);

  DCHECK_EQ(result_images.size(), result_original_image_sizes.size());

  std::move(callback).Run(http_status_code, result_images,
                          result_original_image_sizes);
}

void ImageDownloaderImpl::Dispose() {
  receiver_.reset();
}

void ImageDownloaderImpl::FetchImage(const KURL& image_url,
                                     bool is_favicon,
                                     const gfx::Size& preferred_size,
                                     bool bypass_cache,
                                     DownloadCallback callback) {
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(
      std::make_unique<MultiResolutionImageResourceFetcher>(
          image_url, GetSupplementable(), is_favicon,
          bypass_cache ? blink::mojom::FetchCacheMode::kBypassCache
                       : blink::mojom::FetchCacheMode::kDefault,
          WTF::BindOnce(&ImageDownloaderImpl::DidFetchImage,
                        WrapPersistent(this), std::move(callback),
                        preferred_size)));
}

void ImageDownloaderImpl::DidFetchImage(
    DownloadCallback callback,
    const gfx::Size& preferred_size,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::string& image_data,
    const WebString& mime_type) {
  int32_t http_status_code = fetcher->http_status_code();

  Vector<SkBitmap> images =
      DecodeImageData(image_data, mime_type.Utf8(), preferred_size);

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  for (auto it = image_fetchers_.begin(); it != image_fetchers_.end(); ++it) {
    MultiResolutionImageResourceFetcher* image_fetcher = it->get();
    DCHECK(image_fetcher);
    if (image_fetcher == fetcher) {
      it = image_fetchers_.erase(it);
      break;
    }
  }

  // |this| may be destructed after callback is run.
  std::move(callback).Run(http_status_code, images);
}

void ImageDownloaderImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<LocalFrame>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ImageDownloaderImpl::ContextDestroyed() {
  for (const auto& fetcher : image_fetchers_) {
    // Will run callbacks with an empty image vector.
    fetcher->Dispose();
  }
  image_fetchers_.clear();
}

}  // namespace blink
