// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/image_downloader/image_downloader_base.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"
#include "net/base/data_url.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

using blink::WebFrame;
using blink::WebURLRequest;

namespace {

// Decodes a data: URL image or returns an empty image in case of failure.
SkBitmap ImageFromDataUrl(const GURL& url) {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the image using Blink's image decoder.
    content::ImageDecoder decoder(
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(data.data());

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

}  // namespace

namespace content {

ImageDownloaderBase::ImageDownloaderBase(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  RenderThread::Get()->AddObserver(this);
}

ImageDownloaderBase::~ImageDownloaderBase() {
  RenderThread* thread = RenderThread::Get();
  // The destructor may run after message loop shutdown, so we need to check
  // whether RenderThread is null.
  if (thread)
    thread->RemoveObserver(this);
}

void ImageDownloaderBase::DownloadImage(const GURL& image_url,
                                        bool is_favicon,
                                        bool bypass_cache,
                                        DownloadCallback callback) {
  if (!image_url.SchemeIs(url::kDataScheme)) {
    FetchImage(image_url, is_favicon, bypass_cache, std::move(callback));
    // Will complete asynchronously via ImageDownloaderBase::DidFetchImage.
    return;
  }

  std::vector<SkBitmap> result_images;
  SkBitmap data_image = ImageFromDataUrl(image_url);

  // Drop null or empty SkBitmap.
  if (!data_image.drawsNothing())
    result_images.push_back(data_image);

  std::move(callback).Run(0, result_images);
}

void ImageDownloaderBase::FetchImage(const GURL& image_url,
                                     bool is_favicon,
                                     bool bypass_cache,
                                     DownloadCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  DCHECK(frame);

  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(
      std::make_unique<MultiResolutionImageResourceFetcher>(
          image_url, frame, 0,
          is_favicon ? blink::mojom::RequestContextType::FAVICON
                     : blink::mojom::RequestContextType::IMAGE,
          bypass_cache ? blink::mojom::FetchCacheMode::kBypassCache
                       : blink::mojom::FetchCacheMode::kDefault,
          base::BindOnce(&ImageDownloaderBase::DidFetchImage,
                         base::Unretained(this), std::move(callback))));
}

void ImageDownloaderBase::DidFetchImage(
    DownloadCallback callback,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  int32_t http_status_code = fetcher->http_status_code();

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  for (auto iter = image_fetchers_.begin(); iter != image_fetchers_.end();
       ++iter) {
    if (iter->get() == fetcher) {
      iter->release();
      image_fetchers_.erase(iter);
      render_frame()
          ->GetTaskRunner(blink::TaskType::kInternalLoading)
          ->DeleteSoon(FROM_HERE, fetcher);
      break;
    }
  }

  // |this| may be destructed after callback is run.
  std::move(callback).Run(http_status_code, images);
}

void ImageDownloaderBase::OnDestruct() {
  for (const auto& fetchers : image_fetchers_) {
    // Will run callbacks with an empty image vector.
    fetchers->OnRenderFrameDestruct();
  }
}

}  // namespace content
