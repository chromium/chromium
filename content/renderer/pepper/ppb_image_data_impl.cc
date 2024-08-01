// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_image_data_impl.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/notreached.h"
#include "content/common/pepper_file_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/surface/transport_dib.h"

using ppapi::thunk::PPB_ImageData_API;

namespace content {

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance,
                                       PPB_ImageData_Shared::ImageDataType type)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
  switch (type) {
    case PPB_ImageData_Shared::PLATFORM:
      backend_ = std::make_unique<ImageDataPlatformBackend>();
      return;
    case PPB_ImageData_Shared::SIMPLE:
      backend_ = std::make_unique<ImageDataSimpleBackend>();
      return;
      // No default: so that we get a compiler warning if any types are added.
  }
  NOTREACHED_IN_MIGRATION();
}

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance, ForTest)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
  backend_ = std::make_unique<ImageDataPlatformBackend>();
}

PPB_ImageData_Impl::~PPB_ImageData_Impl() {}

bool PPB_ImageData_Impl::Init(PP_ImageDataFormat format,
                              int width,
                              int height,
                              bool init_to_zero) {
  // TODO(brettw) this should be called only on the main thread!
  if (!IsImageDataFormatSupported(format))
    return false;  // Only support this one format for now.
  if (width <= 0 || height <= 0)
    return false;
  if (static_cast<int64_t>(width) * static_cast<int64_t>(height) >=
      std::numeric_limits<int32_t>::max() / 4)
    return false;  // Prevent overflow of signed 32-bit ints.

  format_ = format;
  width_ = width;
  height_ = height;
  return backend_->Init(this, format, width, height, init_to_zero);
}

// static
PP_Resource PPB_ImageData_Impl::Create(PP_Instance instance,
                                       PPB_ImageData_Shared::ImageDataType type,
                                       PP_ImageDataFormat format,
                                       const PP_Size& size,
                                       PP_Bool init_to_zero) {
  scoped_refptr<PPB_ImageData_Impl> data(
      new PPB_ImageData_Impl(instance, type));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

PPB_ImageData_API* PPB_ImageData_Impl::AsPPB_ImageData_API() { return this; }

bool PPB_ImageData_Impl::IsMapped() const { return backend_->IsMapped(); }

TransportDIB* PPB_ImageData_Impl::GetTransportDIB() const {
  return backend_->GetTransportDIB();
}

PP_Bool PPB_ImageData_Impl::Describe(PP_ImageDataDesc* desc) {
  desc->format = format_;
  desc->size.width = width_;
  desc->size.height = height_;
  desc->stride = width_ * 4;
  return PP_TRUE;
}

void* PPB_ImageData_Impl::Map() { return backend_->Map(); }

void PPB_ImageData_Impl::Unmap() { backend_->Unmap(); }

int32_t PPB_ImageData_Impl::GetSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion** region) {
  return backend_->GetSharedMemoryRegion(region);
}

SkCanvas* PPB_ImageData_Impl::GetCanvas() { return backend_->GetCanvas(); }

void PPB_ImageData_Impl::SetIsCandidateForReuse() {
  // Nothing to do since we don't support image data re-use in-process.
}

SkBitmap PPB_ImageData_Impl::GetMappedBitmap() const {
  return backend_->GetMappedBitmap();
}

// ImageDataPlatformBackend ----------------------------------------------------

ImageDataPlatformBackend::ImageDataPlatformBackend() : width_(0), height_(0) {
}

ImageDataPlatformBackend::~ImageDataPlatformBackend() {
}

bool ImageDataPlatformBackend::Init(PPB_ImageData_Impl* impl,
                                    PP_ImageDataFormat format,
                                    int width,
                                    int height,
                                    bool init_to_zero) {
  // TODO(brettw): use init_to_zero when we implement caching.
  width_ = width;
  height_ = height;
  uint32_t buffer_size = width_ * height_ * 4;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!region.IsValid())
    return false;

  dib_ = TransportDIB::CreateWithHandle(std::move(region));
  return !!dib_;
}

bool ImageDataPlatformBackend::IsMapped() const {
  return !!mapped_canvas_.get();
}

TransportDIB* ImageDataPlatformBackend::GetTransportDIB() const {
  return dib_.get();
}

void* ImageDataPlatformBackend::Map() {
  if (!mapped_canvas_) {
    const bool is_opaque = false;
    mapped_canvas_ = dib_->GetPlatformCanvas(width_, height_, is_opaque);
    if (!mapped_canvas_)
      return nullptr;
  }
  SkPixmap pixmap;
  skia::GetWritablePixels(mapped_canvas_.get(), &pixmap);
  DCHECK(pixmap.addr());
  // SkPixmap does not manage the lifetime of this pointer, so it remains
  // valid after the object goes out of scope. It will become invalid if
  // the canvas' backing is destroyed or a pending saveLayer() is resolved.
  return pixmap.writable_addr32(0, 0);
}

void ImageDataPlatformBackend::Unmap() {
  // This is currently unimplemented, which is OK. The data will just always
  // be around once it's mapped. Chrome's TransportDIB isn't currently
  // unmappable without freeing it, but this may be something we want to support
  // in the future to save some memory.
}

int32_t ImageDataPlatformBackend::GetSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion** region) {
  *region = dib_->shared_memory_region();
  return PP_OK;
}

SkCanvas* ImageDataPlatformBackend::GetCanvas() { return mapped_canvas_.get(); }

SkBitmap ImageDataPlatformBackend::GetMappedBitmap() const {
  SkBitmap bitmap;
  if (!mapped_canvas_)
    return bitmap;

  SkPixmap pixmap;
  skia::GetWritablePixels(mapped_canvas_.get(), &pixmap);
  // SkPixmap does not manage the lifetime of this pointer, so it remains
  // valid after the object goes out of scope. It will become invalid if
  // the canvas' backing is destroyed or a pending saveLayer() is resolved.
  bitmap.installPixels(pixmap);
  return bitmap;
}

// ImageDataSimpleBackend ------------------------------------------------------

ImageDataSimpleBackend::ImageDataSimpleBackend() : map_count_(0) {}

ImageDataSimpleBackend::~ImageDataSimpleBackend() {}

bool ImageDataSimpleBackend::Init(PPB_ImageData_Impl* impl,
                                  PP_ImageDataFormat format,
                                  int width,
                                  int height,
                                  bool init_to_zero) {
  skia_bitmap_.setInfo(
      SkImageInfo::MakeN32Premul(impl->width(), impl->height()));
  shm_region_ =
      base::UnsafeSharedMemoryRegion::Create(skia_bitmap_.computeByteSize());
  return shm_region_.IsValid();
}

bool ImageDataSimpleBackend::IsMapped() const {
  return shm_mapping_.IsValid();
}

TransportDIB* ImageDataSimpleBackend::GetTransportDIB() const {
  return nullptr;
}

void* ImageDataSimpleBackend::Map() {
  DCHECK(shm_region_.IsValid());
  if (map_count_++ == 0) {
    shm_mapping_ = shm_region_.Map();
    if (!shm_mapping_.IsValid())
      return nullptr;

    base::span<uint8_t> mem(shm_mapping_);
    CHECK_GE(mem.size(), skia_bitmap_.computeByteSize());
    skia_bitmap_.setPixels(mem.data());
    // Our platform bitmaps are set to opaque by default, which we don't want.
    skia_bitmap_.setAlphaType(kPremul_SkAlphaType);
    skia_canvas_ = std::make_unique<SkCanvas>(
        skia_bitmap_, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
  }
  return skia_bitmap_.isNull() ? nullptr : skia_bitmap_.getAddr32(0, 0);
}

void ImageDataSimpleBackend::Unmap() {
  if (--map_count_ == 0)
    shm_mapping_ = base::WritableSharedMemoryMapping();
}

int32_t ImageDataSimpleBackend::GetSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion** region) {
  *region = &shm_region_;
  return PP_OK;
}

SkCanvas* ImageDataSimpleBackend::GetCanvas() {
  if (!IsMapped())
    return nullptr;
  return skia_canvas_.get();
}

SkBitmap ImageDataSimpleBackend::GetMappedBitmap() const {
  if (!IsMapped())
    return SkBitmap();
  return skia_bitmap_;
}

}  // namespace content
