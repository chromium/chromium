// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADED_ICON_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADED_ICON_LOADER_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class ResourceRequest;
class SegmentReader;

// Utility class for loading, decoding, and potentially rescaling an icon on a
// background thread. Note that icons are only downscaled and never upscaled.
class CORE_EXPORT ThreadedIconLoader final
    : public GarbageCollected<ThreadedIconLoader>,
      public ThreadableLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadedIconLoader);

 public:
  // On failure, |callback| is called with a null SkBitmap and |resize_scale|
  // set to -1. On success, the icon is provided with a |resize_scale| <= 1.
  using IconCallback =
      base::OnceCallback<void(SkBitmap icon, double resize_scale)>;

  // Starts a background task to download and decode the icon.
  // If |resize_dimensions| is provided, the icon will will be downscaled to
  // those dimensions.
  void Start(ExecutionContext* execution_context,
             const ResourceRequest& resource_request,
             const base::Optional<WebSize>& resize_dimensions,
             IconCallback callback);

  // Stops the background task. The provided callback will not be run if
  // `Stop` is called.
  void Stop();

  // ThreadableLoaderClient interface.
  void DidReceiveData(const char* data, unsigned length) override;
  void DidFinishLoading(uint64_t resource_identifier) override;
  void DidFail(const ResourceError& error) override;
  void DidFailRedirectCheck() override;

  void Trace(blink::Visitor* visitor) override;

 private:
  void DecodeAndResizeImageOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<SegmentReader> data);

  void OnBackgroundTaskComplete(double resize_scale);

  Member<ThreadableLoader> threadable_loader_;

  // Data received from |threadable_loader_|. Will be invalidated when decoding
  // of the image data starts.
  scoped_refptr<SharedBuffer> data_;

  // Accessed from main thread and background thread.
  base::Optional<WebSize> resize_dimensions_;
  SkBitmap decoded_icon_;

  IconCallback icon_callback_;

  base::TimeTicks start_time_;

  bool stopped_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_THREADED_ICON_LOADER_H_
