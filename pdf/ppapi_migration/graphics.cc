// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <stdint.h>

#include <cmath>
#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/image.h"
#include "pdf/ppapi_migration/result_codes.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

Graphics::Graphics(const gfx::Size& size) : size_(size) {}

PepperGraphics::PepperGraphics(const pp::InstanceHandle& instance,
                               const gfx::Size& size)
    : Graphics(size),
      pepper_graphics_(instance,
                       PPSizeFromSize(size),
                       /*is_always_opaque=*/true) {}

PepperGraphics::~PepperGraphics() = default;

bool PepperGraphics::Flush(ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_graphics_.Flush(pp_callback);
  if (result == PP_OK_COMPLETIONPENDING) {
    return true;
  }

  // Should only happen if pp::Graphics2D::Flush() is called while a callback is
  // still pending, which should never happen if PaintManager is managing all
  // flushes.
  DCHECK_EQ(Result::kSuccess, result);
  pp_callback.Run(result);
  return false;
}

void PepperGraphics::PaintImage(const Image& image, const gfx::Rect& src_rect) {
  pepper_graphics_.PaintImageData(image.pepper_image(), pp::Point(),
                                  PPRectFromRect(src_rect));
}

void PepperGraphics::Scroll(const gfx::Rect& clip,
                            const gfx::Vector2d& amount) {
  pepper_graphics_.Scroll(PPRectFromRect(clip),
                          pp::Point(amount.x(), amount.y()));
}

void PepperGraphics::SetScale(float scale) {
  bool result = pepper_graphics_.SetScale(scale);
  DCHECK(result);
}

void PepperGraphics::SetLayerTransform(float scale,
                                       const gfx::Point& origin,
                                       const gfx::Vector2d& translate) {
  bool result = pepper_graphics_.SetLayerTransform(
      scale, pp::Point(origin.x(), origin.y()),
      pp::Point(translate.x(), translate.y()));
  DCHECK(result);
}

// static
std::unique_ptr<SkiaGraphics> SkiaGraphics::Create(Client* client,
                                                   const gfx::Size& size) {
  auto graphics = base::WrapUnique(new SkiaGraphics(client, size));
  if (!graphics->skia_graphics_)
    return nullptr;

  return graphics;
}

SkiaGraphics::SkiaGraphics(Client* client, const gfx::Size& size)
    : Graphics(size),
      client_(client),
      skia_graphics_(
          SkSurface::MakeRasterN32Premul(size.width(), size.height())) {}

SkiaGraphics::~SkiaGraphics() = default;

// TODO(https://crbug.com/1099020): After completely switching to non-Pepper
// plugin, make Flush() return false since there is no pending action for
// syncing the client's snapshot.
bool SkiaGraphics::Flush(ResultCallback callback) {
  sk_sp<SkImage> snapshot = skia_graphics_->makeImageSnapshot();
  skia_graphics_->getCanvas()->drawImage(
      snapshot.get(), /*x=*/0, /*y=*/0, SkSamplingOptions(), /*paint=*/nullptr);

  client_->UpdateSnapshot(std::move(snapshot));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
  return true;
}

void SkiaGraphics::PaintImage(const Image& image, const gfx::Rect& src_rect) {
  SkRect skia_rect = RectToSkRect(src_rect);
  skia_graphics_->getCanvas()->drawImageRect(
      image.skia_image().asImage(), skia_rect, skia_rect, SkSamplingOptions(),
      nullptr, SkCanvas::kStrict_SrcRectConstraint);
}

void SkiaGraphics::Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) {
  // If we are being asked to scroll by more than the graphics' rect size, just
  // ignore the scroll command.
  if (std::abs(amount.x()) >= skia_graphics_->width() ||
      std::abs(amount.y()) >= skia_graphics_->height()) {
    return;
  }

  // TODO(crbug.com/1263614): Use `SkSurface::notifyContentWillChange()`.
  gfx::ScrollCanvas(skia_graphics_->getCanvas(), clip, amount);
}

void SkiaGraphics::SetScale(float scale) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  client_->UpdateScale(scale);
}

void SkiaGraphics::SetLayerTransform(float scale,
                                     const gfx::Point& origin,
                                     const gfx::Vector2d& translate) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  // translate_with_origin = origin - scale * origin - translate
  gfx::Vector2dF translate_with_origin = origin.OffsetFromOrigin();
  translate_with_origin.Scale(1.0f - scale);
  translate_with_origin.Subtract(translate);

  // TODO(crbug.com/1263614): Pepper defers updates until `Flush()`. Determine
  // if `SkiaGraphics` should do something similar.
  client_->UpdateLayerTransform(scale, translate_with_origin);
}

}  // namespace chrome_pdf
