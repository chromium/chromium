// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_GRAPHICS_CONTEXT_3D_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_GRAPHICS_CONTEXT_3D_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrTexture.h"

typedef unsigned int GLenum;

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT GraphicsContext3DUtils {
  USING_FAST_MALLOC(GraphicsContext3DUtils);

 public:
  // The constructor takes a weak ref to the wrapper because it internally
  // it generates callbacks that may outlive the wrapper.
  GraphicsContext3DUtils(base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
                             context_provider_wrapper)
      : context_provider_wrapper_(std::move(context_provider_wrapper)) {}

  // Use this service to create a new mailbox or possibly obtain a pre-existing
  // mailbox for a given texture. The caching of pre-existing mailboxes survives
  // when the texture gets recycled by skia for creating a new SkSurface or
  // SkImage with a pre-existing GrTexture backing.
  bool GetMailboxForSkImage(gpu::Mailbox&,
                            GLenum&,
                            const sk_sp<SkImage>&,
                            GLenum filter);
  void RegisterMailbox(GrTexture*, const gpu::Mailbox&);
  void RemoveCachedMailbox(GrTexture*);

  bool Accelerated2DCanvasFeatureEnabled();

 private:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  WTF::HashMap<GrTexture*, gpu::Mailbox> cached_mailboxes_;
};

}  // namespace blink

#endif
