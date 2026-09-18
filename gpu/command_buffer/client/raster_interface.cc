// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_interface.h"

#include <utility>

#include "base/check.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gpu::raster {

RasterInterface::CopySharedImageResult RasterInterface::CopySharedImage(
    const scoped_refptr<ClientSharedImage>& source,
    const SyncToken& source_sync_token,
    const scoped_refptr<ClientSharedImage>& dest,
    const SyncToken& dest_sync_token,
    const gfx::Rect& source_rect,
    const gfx::Point& dest_offset) {
  CHECK(source);
  CHECK(dest);
  auto dst_access =
      dest->BeginRasterAccess(this, dest_sync_token, /*readonly=*/false);
  auto src_access =
      source->BeginRasterAccess(this, source_sync_token, /*readonly=*/true);
  CopySharedImage(source->mailbox(), dest->mailbox(), dest_offset.x(),
                  dest_offset.y(), source_rect.x(), source_rect.y(),
                  source_rect.width(), source_rect.height());
  auto src_completion_token =
      RasterScopedAccess::EndAccess(std::move(src_access));
  source->UpdateDestructionSyncToken(src_completion_token);
  auto dst_completion_token =
      RasterScopedAccess::EndAccess(std::move(dst_access));
  dest->UpdateDestructionSyncToken(dst_completion_token);
  return {src_completion_token, dst_completion_token};
}

SyncToken RasterInterface::WritePixels(
    const scoped_refptr<ClientSharedImage>& dest,
    const SyncToken& sync_token,
    int dst_x_offset,
    int dst_y_offset,
    const SkPixmap& src_sk_pixmap) {
  CHECK(dest);
  auto access = dest->BeginRasterAccess(this, sync_token, /*readonly=*/false);
  WritePixels(dest->mailbox(), dst_x_offset, dst_y_offset,
              dest->GetTextureTarget(), src_sk_pixmap);
  auto new_token = RasterScopedAccess::EndAccess(std::move(access));
  dest->UpdateDestructionSyncToken(new_token);
  return new_token;
}

SyncToken RasterInterface::RasterSharedImage(
    const scoped_refptr<ClientSharedImage>& dest,
    const SyncToken& sync_token,
    cc::PaintRecord record,
    cc::ImageProvider* image_provider,
    bool needs_clear,
    base::RepeatingCallback<void(SkCanvas*, uint32_t)> custom_callback) {
  CHECK(dest);
  auto access = dest->BeginRasterAccess(this, sync_token, /*readonly=*/false);

  SkColor4f background_color = dest->alpha_type() == kOpaque_SkAlphaType
                                   ? SkColors::kBlack
                                   : SkColors::kTransparent;

  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  list->StartPaint();
  list->push<cc::DrawRecordOp>(std::move(record));
  list->EndPaintOfUnpaired(gfx::Rect(dest->size()));
  list->Finalize();

  gfx::Size size = dest->size();
  size_t max_op_size_hint = kDefaultMaxOpSizeHint;
  gfx::Rect full_raster_rect(dest->size());
  gfx::Rect playback_rect(dest->size());
  gfx::Vector2dF post_translate(0.f, 0.f);
  gfx::Vector2dF post_scale(1.f, 1.f);

  const bool can_use_lcd_text = dest->alpha_type() == kOpaque_SkAlphaType;
  const auto& caps = GetCapabilities();
  bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
  BeginRasterCHROMIUM(
      background_color, needs_clear,
      /*msaa_sample_count=*/use_msaa ? 1 : 0,
      use_msaa ? MsaaMode::kDMSAA : MsaaMode::kNoMSAA, can_use_lcd_text,
      /*visible=*/true, dest->color_space(),
      /*hdr_headroom=*/0.f, dest->mailbox().name);

  RasterCHROMIUM(list.get(), image_provider, size, full_raster_rect,
                 playback_rect, post_translate, post_scale,
                 /*requires_clear=*/false,
                 /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint,
                 std::move(custom_callback));

  EndRasterCHROMIUM();
  auto completion_sync_token = RasterScopedAccess::EndAccess(std::move(access));
  dest->UpdateDestructionSyncToken(completion_sync_token);
  return completion_sync_token;
}

}  // namespace gpu::raster
