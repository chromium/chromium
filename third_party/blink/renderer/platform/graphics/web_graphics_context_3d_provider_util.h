// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_UTIL_H_

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// Synchronously creates a WebGraphicsContext3DProvider on a non-main thread.
//
// Returns a newly allocated and initialized offscreen context provider, backed
// by an independent context. Returns null if the context cannot be created or
// initialized.
//
// Upon successful completion, |gl_info| and |using_gpu_compositing| will be
// filled in with their actual values.
//
// A blocking task is posted to the main thread to create the context, so do not
// call this method from code which may block main thread progress.
PLATFORM_EXPORT std::unique_ptr<WebGraphicsContext3DProvider>
CreateContextProviderOnWorkerThread(
    Platform::ContextAttributes context_attributes,
    Platform::GraphicsInfo* gl_info,
    bool* using_gpu_compositing,
    const KURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_UTIL_H_
