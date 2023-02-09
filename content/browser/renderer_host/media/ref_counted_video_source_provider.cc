// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"

#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace content {

RefCountedVideoSourceProvider::RefCountedVideoSourceProvider(
    mojo::Remote<video_capture::mojom::VideoSourceProvider> source_provider,
    base::OnceClosure destruction_cb)
    : source_provider_(std::move(source_provider)),
      destruction_cb_(std::move(destruction_cb)) {}

RefCountedVideoSourceProvider::~RefCountedVideoSourceProvider() {
  std::move(destruction_cb_).Run();
}

base::WeakPtr<RefCountedVideoSourceProvider>
RefCountedVideoSourceProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RefCountedVideoSourceProvider::ReleaseProviderForTesting() {
  source_provider_.reset();
}

}  // namespace content
