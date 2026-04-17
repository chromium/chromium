// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class CanvasResource;
class StaticBitmapImage;

class PLATFORM_EXPORT ExportedCanvasResource
    : public ThreadSafeRefCounted<ExportedCanvasResource> {
 public:
  explicit ExportedCanvasResource(scoped_refptr<CanvasResource> resource);
  ~ExportedCanvasResource();

  static void OnPlaceholderReleasedResource(
      scoped_refptr<ExportedCanvasResource>&& resource);

  gfx::Size Size() const;
  bool OriginClean() const;
  scoped_refptr<StaticBitmapImage> Bitmap();
  void Transfer();

  CanvasResource* GetResourceForTesting() const { return resource_.get(); }

 private:
  scoped_refptr<CanvasResource> resource_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_EXPORTED_CANVAS_RESOURCE_H_
