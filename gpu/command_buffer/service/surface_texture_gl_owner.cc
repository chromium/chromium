// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/surface_texture_gl_owner.h"

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {
namespace {

// Makes |texture_owner|'s context current if it isn't already.
std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded(
    gpu::TextureOwner* texture_owner) {
  gl::GLContext* context = texture_owner->GetContext();
  // Note: this works for virtual contexts too, because IsCurrent() returns true
  // if their shared platform context is current, regardless of which virtual
  // context is current.
  if (context->IsCurrent(nullptr))
    return nullptr;

  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      context, texture_owner->GetSurface());
  // Log an error if ScopedMakeCurrent failed for debugging
  // https://crbug.com/878042.
  // TODO(ericrk): Remove this once debugging is completed.
  if (!context->IsCurrent(nullptr)) {
    LOG(ERROR) << "Failed to make context current in CodecImage. Subsequent "
                  "UpdateTexImage may fail.";
  }
  return scoped_current;
}
}  // namespace

SurfaceTextureGLOwner::SurfaceTextureGLOwner(
    std::unique_ptr<AbstractTextureAndroid> texture,
    scoped_refptr<SharedContextState> context_state)
    : TextureOwner(true /*binds_texture_on_update */,
                   std::move(texture),
                   std::move(context_state)),
      surface_texture_(gl::SurfaceTexture::Create(GetTextureId())),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()) {
  DCHECK(context_);
  DCHECK(surface_);
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Do ReleaseResources() if it hasn't already. This will do nothing if it
  // has already been destroyed.
  ReleaseResources();
}

void SurfaceTextureGLOwner::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Make sure that the SurfaceTexture isn't using the GL objects.
  surface_texture_ = nullptr;
  last_coded_size_for_memory_dumps_.reset();
}

void SurfaceTextureGLOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {
  DCHECK(!is_frame_available_callback_set_);

  // Setting the callback to be run from any thread since |frame_available_cb|
  // is thread safe.
  is_frame_available_callback_set_ = true;
  surface_texture_->SetFrameAvailableCallbackOnAnyThread(frame_available_cb);
}

void SurfaceTextureGLOwner::RunWhenBufferIsAvailable(
    base::OnceClosure callback) {
  // SurfaceTexture can always render to front buffer.
  std::move(callback).Run();
}

gl::ScopedJavaSurface SurfaceTextureGLOwner::CreateJavaSurface() const {
  // |surface_texture_| might be null, but that's okay.
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_) {
    // UpdateTexImage bounds texture to the SurfaceTexture context, so make it
    // current.
    auto scoped_make_current = MakeCurrentIfNeeded(this);
    if (scoped_make_current && !scoped_make_current->IsContextCurrent())
      return;

    // UpdateTexImage might change gl binding and we never should alter gl
    // binding without updating state tracking, which we can't do here, so
    // restore previous after we done.
    gl::ScopedRestoreTexture scoped_restore_texture(gl::g_current_gl_context,
                                                    GL_TEXTURE_EXTERNAL_OES);
    surface_texture_->UpdateTexImage();
  }
}

void SurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_) {
    surface_texture_->ReleaseBackBuffers();
  }
  last_coded_size_for_memory_dumps_.reset();
}

gl::GLContext* SurfaceTextureGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* SurfaceTextureGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SurfaceTextureGLOwner::GetAHardwareBuffer() {
  NOTREACHED_IN_MIGRATION()
      << "Don't use AHardwareBuffers with SurfaceTextureGLOwner";
  return nullptr;
}

bool SurfaceTextureGLOwner::GetCodedSizeAndVisibleRect(
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  if (!surface_texture_) {
    *visible_rect = gfx::Rect();
    *coded_size = gfx::Size();
    return false;
  }

  float mtx[16];
  surface_texture_->GetTransformMatrix(mtx);

  bool result =
      DecomposeTransform(mtx, rotated_visible_size, coded_size, visible_rect);

  constexpr gfx::Rect kMaxRect(16536, 16536);
  gfx::Rect coded_rect(*coded_size);

  if (!result || !coded_rect.Contains(*visible_rect) ||
      !kMaxRect.Contains(coded_rect)) {
    // Save old values for minidump.
    gfx::Size coded_size_for_debug = *coded_size;
    gfx::Rect visible_rect_for_debug = *visible_rect;

    // Sanitize returning values to avoid crashes.
    *coded_size = rotated_visible_size;
    *visible_rect = gfx::Rect(rotated_visible_size);

    // Alias values to prevent optimization out and do logging/dump.
    base::debug::Alias(mtx);
    base::debug::Alias(&coded_size_for_debug);
    base::debug::Alias(&visible_rect_for_debug);

    LOG(ERROR) << "Wrong matrix decomposition: coded: "
               << coded_size_for_debug.ToString()
               << "visible: " << visible_rect_for_debug.ToString()
               << "matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
               << ", " << mtx[5] << ", " << mtx[12] << ", " << mtx[13];

    base::debug::DumpWithoutCrashing();
  }

  last_coded_size_for_memory_dumps_ = *coded_size;

  return true;
}

// static
bool SurfaceTextureGLOwner::DecomposeTransform(float mtx[16],
                                               gfx::Size rotated_visible_size,
                                               gfx::Size* coded_size,
                                               gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  // Due to shrinking of visible rect by 1 pixels from each side matrix can be
  // zero for visible sizes less then 4x4.
  if (rotated_visible_size.width() < 4 || rotated_visible_size.height() < 4) {
    *coded_size = rotated_visible_size;
    *visible_rect = gfx::Rect(rotated_visible_size);

    // Note as this is expected case we return true, to avoid crash dump above.
    return true;
  }

  float sx, sy;
  *visible_rect = gfx::Rect();
  // The matrix is in column order and contains transform of (0, 0)x(1, 1)
  // textur coordinates rect into visible portion of the buffer.

  if (mtx[0]) {
    // If mtx[0] is non zero, mtx[5] must be non-zero while mtx[1] and mtx[4]
    // must be zero if this is 0/180 rotation + scale/translate.
    LOG_IF(DFATAL, mtx[1] || mtx[4] || !mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    // Scale is on diagonal, drop any flips or rotations.
    sx = mtx[0];
    sy = mtx[5];

    // 0/180 degrees doesn't swap width/height
    visible_rect->set_size(rotated_visible_size);
  } else {
    // If mtx[0] is zero, mtx[5] must be zero while mtx[1] and mtx[4] must be
    // non-zero if this is rotation 90/270 rotation + scale/translate.
    LOG_IF(DFATAL, !mtx[1] || !mtx[4] || mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    // Scale is on reverse diagonal for inner 2x2 matrix
    sx = mtx[4];
    sy = mtx[1];

    // Frame is rotated, we need to swap width/height
    visible_rect->set_width(rotated_visible_size.height());
    visible_rect->set_height(rotated_visible_size.width());
  }

  // Read translate and flip them if scale is negative.
  float tx = sx > 0 ? mtx[12] : (sx + mtx[12]);
  float ty = sy > 0 ? mtx[13] : (sy + mtx[13]);

  // Drop scale signs from rotation/flip as it's handled above already
  sx = std::abs(sx);
  sy = std::abs(sy);

  // We got zero matrix, so we can't decompose anything.
  if (!sx || !sy) {
    return false;
  }

  *coded_size = visible_rect->size();

  // Note: Below calculation is reverse operation of computing matrix in
  // SurfaceTexture::computeCurrentTransformMatrix() -
  // https://android.googlesource.com/platform/frameworks/native/+/5c1139f/libs/gui/SurfaceTexture.cpp#516.
  // We are assuming here that bilinear filtering is always enabled for
  // sampling the texture.

  // In order to prevent bilinear sampling beyond the edge of the
  // crop rectangle might have been shrunk by up to 2 texels in each dimension
  // depending on format and if the filtering was enabled. We will try with
  // worst case of YUV as most common and work down.

  const float possible_shrinks_amounts[] = {1.0f, 0.5f, 0.0f};

  for (float shrink_amount : possible_shrinks_amounts) {
    if (sx < 1.0f) {
      coded_size->set_width(
          std::round((visible_rect->width() - 2.0f * shrink_amount) / sx));
      visible_rect->set_x(std::round(tx * coded_size->width() - shrink_amount));
    }
    if (sy < 1.0f) {
      coded_size->set_height(
          std::round((visible_rect->height() - 2.0f * shrink_amount) / sy));
      visible_rect->set_y(
          std::round(ty * coded_size->height() - shrink_amount));
    }

    // If origin of visible_rect is negative we likely trying too big
    // |shrink_amount| so we need to check for next value. Otherwise break the
    // loop.
    if (visible_rect->x() >= 0 && visible_rect->y() >= 0) {
      break;
    }
  }
  return true;
}

bool SurfaceTextureGLOwner::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto dump_name =
      base::StringPrintf("gpu/media_texture_owner_%d", tracing_id());

  // We don't know the exact format of the image so we use NV12 as approximation
  // as the most popular format.
  constexpr auto format = viz::MultiPlaneFormat::kNV12;
  size_t total_size = 0;

  if (last_coded_size_for_memory_dumps_) {
    total_size =
        format.EstimatedSizeInBytes(last_coded_size_for_memory_dumps_.value());
  }

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_size);

  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    dump->AddString(
        "dimensions", "",
        last_coded_size_for_memory_dumps_.value_or(gfx::Size()).ToString());
  }
  return true;
}

}  // namespace gpu
