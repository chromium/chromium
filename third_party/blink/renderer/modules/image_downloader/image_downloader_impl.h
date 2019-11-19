// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class KURL;
class LocalFrame;
class MultiResolutionImageResourceFetcher;
class WebString;

struct WebSize;

class ImageDownloaderImpl final : public GarbageCollected<ImageDownloaderImpl>,
                                  public Supplement<LocalFrame>,
                                  public ContextLifecycleObserver,
                                  public mojom::blink::ImageDownloader {
  USING_PRE_FINALIZER(ImageDownloaderImpl, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(ImageDownloaderImpl);

 public:
  static const char kSupplementName[];

  explicit ImageDownloaderImpl(LocalFrame&);
  ~ImageDownloaderImpl() override;

  using DownloadCallback =
      base::OnceCallback<void(int32_t, const WTF::Vector<SkBitmap>&)>;

  static ImageDownloaderImpl* From(LocalFrame&);

  static void ProvideTo(LocalFrame&);

  void Trace(Visitor*) override;

  // OverContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

 private:
  // ImageDownloader implementation. Request to asynchronously download an
  // image. When done, |callback| will be called.
  void DownloadImage(const KURL& url,
                     bool is_favicon,
                     uint32_t preferred_size,
                     uint32_t max_bitmap_size,
                     bool bypass_cache,
                     DownloadImageCallback callback) override;

  // Called when downloading finishes. All frames in |images| whose size <=
  // |max_image_size| will be returned through |callback|. If all of the frames
  // are larger than |max_image_size|, the smallest frame is resized to
  // |max_image_size| and is the only result. |max_image_size| == 0 is
  // interpreted as no max image size.
  void DidDownloadImage(uint32_t max_bitmap_size,
                        DownloadImageCallback callback,
                        int32_t http_status_code,
                        const WTF::Vector<SkBitmap>& images);

  void CreateMojoService(
      mojo::PendingReceiver<mojom::blink::ImageDownloader> receiver);

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void Dispose();

  // Requests to fetch an image. When done, the image downloader is notified by
  // way of DidFetchImage. If the image is a favicon, cookies will not be sent
  // nor accepted during download. If the image has multiple frames, all frames
  // are returned.
  void FetchImage(const KURL& image_url,
                  bool is_favicon,
                  const WebSize& preferred_size,
                  bool bypass_cache,
                  DownloadCallback callback);

  // This callback is triggered when FetchImage completes, either
  // successfully or with a failure. See FetchImage for more
  // details.
  void DidFetchImage(DownloadCallback callback,
                     const WebSize& preferred_size,
                     MultiResolutionImageResourceFetcher* fetcher,
                     const std::string& image_data,
                     const WebString& mime_type);

  typedef WTF::Vector<std::unique_ptr<MultiResolutionImageResourceFetcher>>
      ImageResourceFetcherList;

  // ImageResourceFetchers schedule via FetchImage.
  ImageResourceFetcherList image_fetchers_;

  mojo::Receiver<mojom::blink::ImageDownloader> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageDownloaderImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
