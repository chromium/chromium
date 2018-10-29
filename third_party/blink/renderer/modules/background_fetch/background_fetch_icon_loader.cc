// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LiICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"

#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/manifest/image_resource.h"
#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {

constexpr unsigned long kIconFetchTimeoutInMs = 30000;
constexpr int kMinimumIconSizeInPx = 0;

// Because including base::ClampToRange would be a dependency violation.
int ClampToRange(const int value, const int min, const int max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

BackgroundFetchIconLoader::BackgroundFetchIconLoader() = default;
BackgroundFetchIconLoader::~BackgroundFetchIconLoader() {
  // We should've called Stop() before the destructor is invoked.
  DCHECK(stopped_ || icon_callback_.is_null());
}

void BackgroundFetchIconLoader::Start(BackgroundFetchBridge* bridge,
                                      ExecutionContext* execution_context,
                                      HeapVector<ManifestImageResource> icons,
                                      IconCallback icon_callback) {
  DCHECK(!stopped_);
  DCHECK_GE(icons.size(), 1u);
  DCHECK(bridge);

  icons_ = std::move(icons);
  bridge->GetIconDisplaySize(
      WTF::Bind(&BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon,
                WrapWeakPersistent(this), WrapWeakPersistent(execution_context),
                std::move(icon_callback)));
}

void BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon(
    ExecutionContext* execution_context,
    IconCallback icon_callback,
    const WebSize& icon_display_size_pixels) {
  icon_display_size_pixels_ = icon_display_size_pixels;

  // If |icon_display_size_pixels_| is empty then no image will be displayed by
  // the UI powering Background Fetch. Bail out immediately.
  if (icon_display_size_pixels_.IsEmpty()) {
    std::move(icon_callback)
        .Run(SkBitmap(), -1 /* ideal_to_chosen_icon_size_times_hundred */);
    return;
  }

  KURL best_icon_url = PickBestIconForDisplay(execution_context);
  if (best_icon_url.IsEmpty()) {
    // None of the icons provided was suitable.
    std::move(icon_callback)
        .Run(SkBitmap(), -1 /* ideal_to_chosen_icon_size_times_hundred */);
    return;
  }

  icon_callback_ = std::move(icon_callback);

  ResourceLoaderOptions resource_loader_options;
  if (execution_context->IsWorkerGlobalScope())
    resource_loader_options.request_initiator_context = kWorkerContext;

  ResourceRequest resource_request(best_icon_url);
  resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetKeepalive(true);
  resource_request.SetFetchRequestMode(
      network::mojom::FetchRequestMode::kNoCORS);
  resource_request.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kInclude);
  resource_request.SetSkipServiceWorker(true);

  threadable_loader_ =
      new ThreadableLoader(*execution_context, this, resource_loader_options);
  threadable_loader_->SetTimeout(
      TimeDelta::FromMilliseconds(kIconFetchTimeoutInMs));
  threadable_loader_->Start(resource_request);
}

KURL BackgroundFetchIconLoader::PickBestIconForDisplay(
    ExecutionContext* execution_context) {
  std::vector<Manifest::ImageResource> icons;
  for (auto& icon : icons_) {
    // Update the src of |icon| to include the base URL in case relative paths
    // were used.
    icon.setSrc(execution_context->CompleteURL(icon.src()));
    Manifest::ImageResource candidate_icon =
        blink::ConvertManifestImageResource(icon);
    // Provide default values for 'purpose' and 'sizes' if they are missing.
    if (candidate_icon.sizes.empty())
      candidate_icon.sizes.emplace_back(gfx::Size(0, 0));
    if (candidate_icon.purpose.empty()) {
      candidate_icon.purpose.emplace_back(
          Manifest::ImageResource::Purpose::ANY);
    }
    icons.emplace_back(candidate_icon);
  }

  return KURL(ManifestIconSelector::FindBestMatchingIcon(
      std::move(icons), icon_display_size_pixels_.height, kMinimumIconSizeInPx,
      Manifest::ImageResource::Purpose::ANY));
}

void BackgroundFetchIconLoader::Stop() {
  if (stopped_)
    return;

  stopped_ = true;
  if (threadable_loader_) {
    threadable_loader_->Cancel();
    threadable_loader_ = nullptr;
  }
}

void BackgroundFetchIconLoader::DidReceiveData(const char* data,
                                               unsigned length) {
  if (!data_)
    data_ = SharedBuffer::Create();
  data_->Append(data, length);
}

void BackgroundFetchIconLoader::DidFinishLoading(
    unsigned long resource_identifier) {
  if (stopped_)
    return;

  if (!data_) {
    RunCallbackWithEmptyBitmap();
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Platform::Current()->CurrentThread()->GetTaskRunner();

  background_scheduler::PostOnBackgroundThread(
      FROM_HERE,
      CrossThreadBind(
          &BackgroundFetchIconLoader::DecodeAndResizeImageOnBackgroundThread,
          WrapCrossThreadPersistent(this), std::move(task_runner),
          SegmentReader::CreateFromSharedBuffer(std::move(data_))));
}

void BackgroundFetchIconLoader::DecodeAndResizeImageOnBackgroundThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<SegmentReader> data) {
  DCHECK(task_runner);
  DCHECK(data);

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      std::move(data), /* data_complete= */ true,
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::TransformToSRGB());

  int64_t ideal_to_chosen_icon_size_times_hundred = -1;
  if (decoder) {
    ImageFrame* image_frame = decoder->DecodeFrameBufferAtIndex(0);
    if (image_frame) {
      decoded_icon_ = image_frame->Bitmap();

      int width = decoded_icon_.width();
      int height = decoded_icon_.height();

      // If the |decoded_icon_| is larger than |icon_display_size_pixels_|
      // permits, we need to resize it as well. This can be done synchronously
      // given that we're on a background thread already.
      double scale = std::min(
          static_cast<double>(icon_display_size_pixels_.width) / width,
          static_cast<double>(icon_display_size_pixels_.height) / height);

      ideal_to_chosen_icon_size_times_hundred = std::round(scale * 100.0);

      if (scale < 1) {
        width = ClampToRange(scale * width, 1, icon_display_size_pixels_.width);
        height =
            ClampToRange(scale * height, 1, icon_display_size_pixels_.height);

        // Use the RESIZE_GOOD quality allowing the implementation to pick an
        // appropriate method for the resize. Can be increased to RESIZE_BETTER
        // or RESIZE_BEST if the quality looks poor.
        decoded_icon_ = skia::ImageOperations::Resize(
            decoded_icon_, skia::ImageOperations::RESIZE_GOOD, width, height);
      }
    }
  }

  PostCrossThreadTask(*task_runner, FROM_HERE,
                      CrossThreadBind(&BackgroundFetchIconLoader::RunCallback,
                                      WrapCrossThreadPersistent(this),
                                      ideal_to_chosen_icon_size_times_hundred));
}

void BackgroundFetchIconLoader::DidFail(const ResourceError& error) {
  RunCallbackWithEmptyBitmap();
}

void BackgroundFetchIconLoader::DidFailRedirectCheck() {
  RunCallbackWithEmptyBitmap();
}

void BackgroundFetchIconLoader::RunCallback(
    int64_t ideal_to_chosen_icon_size_times_hundred) {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (stopped_)
    return;

  std::move(icon_callback_)
      .Run(decoded_icon_, ideal_to_chosen_icon_size_times_hundred);
}

void BackgroundFetchIconLoader::RunCallbackWithEmptyBitmap() {
  DCHECK(decoded_icon_.isNull());
  RunCallback(-1 /* ideal_to_chosen_icon_size_times_hundred */);
}

}  // namespace blink
