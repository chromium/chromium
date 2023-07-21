// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_IMAGE_DATA_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_IMAGE_DATA_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;
class TransportDIB;

namespace content {

class PPB_ImageData_Impl : public ppapi::Resource,
                           public ppapi::PPB_ImageData_Shared,
                           public ppapi::thunk::PPB_ImageData_API {
 public:
  // We delegate most of our implementation to a back-end class that either uses
  // a PlatformCanvas (for most trusted stuff) or bare shared memory (for use by
  // NaCl, or trusted plugins when the PlatformCanvas isn't needed). This makes
  // it cheap & easy to implement Swap.
  class Backend {
   public:
    virtual ~Backend() {}
    virtual bool Init(PPB_ImageData_Impl* impl,
                      PP_ImageDataFormat format,
                      int width,
                      int height,
                      bool init_to_zero) = 0;
    virtual bool IsMapped() const = 0;
    virtual TransportDIB* GetTransportDIB() const = 0;
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
    virtual int32_t GetSharedMemoryRegion(
        base::UnsafeSharedMemoryRegion** region) = 0;
    virtual SkCanvas* GetCanvas() = 0;
    virtual SkBitmap GetMappedBitmap() const = 0;
  };

  // If you call this constructor, you must also call Init before use. Normally
  // you should use the static Create function, but this constructor is needed
  // for some internal uses of ImageData (like Graphics2D).
  PPB_ImageData_Impl(PP_Instance instance,
                     PPB_ImageData_Shared::ImageDataType type);

  PPB_ImageData_Impl(const PPB_ImageData_Impl&) = delete;
  PPB_ImageData_Impl& operator=(const PPB_ImageData_Impl&) = delete;

  // Constructor used for unittests. The ImageData is always allocated locally.
  struct ForTest {};
  PPB_ImageData_Impl(PP_Instance instance, ForTest);

  bool Init(PP_ImageDataFormat format,
            int width,
            int height,
            bool init_to_zero);

  static PP_Resource Create(PP_Instance pp_instance,
                            PPB_ImageData_Shared::ImageDataType type,
                            PP_ImageDataFormat format,
                            const PP_Size& size,
                            PP_Bool init_to_zero);

  int width() const { return width_; }
  int height() const { return height_; }

  // Returns the image format.
  PP_ImageDataFormat format() const { return format_; }

  // Returns true if this image is mapped. False means that the image is either
  // invalid or not mapped. See ImageDataAutoMapper below.
  bool IsMapped() const;
  TransportDIB* GetTransportDIB() const;

  // Resource override.
  ppapi::thunk::PPB_ImageData_API* AsPPB_ImageData_API() override;

  // PPB_ImageData_API implementation.
  PP_Bool Describe(PP_ImageDataDesc* desc) override;
  void* Map() override;
  void Unmap() override;
  int32_t GetSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion** region) override;
  SkCanvas* GetCanvas() override;
  void SetIsCandidateForReuse() override;

  // Returns an *empty* bitmap on error.
  // Users must call SkBitmap::lockPixels() before SkBitmap::getPixels();
  // unlockPixels() will be automatically invoked if necessary when the
  // bitmap goes out of scope.
  SkBitmap GetMappedBitmap() const;

 private:
  ~PPB_ImageData_Impl() override;

  PP_ImageDataFormat format_;
  int width_;
  int height_;
  std::unique_ptr<Backend> backend_;
};

class ImageDataPlatformBackend : public PPB_ImageData_Impl::Backend {
 public:
  // |is_browser_allocated| indicates whether the backing shared memory should
  // be allocated by the browser process.
  ImageDataPlatformBackend();

  ImageDataPlatformBackend(const ImageDataPlatformBackend&) = delete;
  ImageDataPlatformBackend& operator=(const ImageDataPlatformBackend&) = delete;

  ~ImageDataPlatformBackend() override;

  // PPB_ImageData_Impl::Backend implementation.
  bool Init(PPB_ImageData_Impl* impl,
            PP_ImageDataFormat format,
            int width,
            int height,
            bool init_to_zero) override;
  bool IsMapped() const override;
  TransportDIB* GetTransportDIB() const override;
  void* Map() override;
  void Unmap() override;
  int32_t GetSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion** region) override;
  SkCanvas* GetCanvas() override;
  SkBitmap GetMappedBitmap() const override;

 private:
  // This will be NULL before initialization, and if this PPB_ImageData_Impl is
  // swapped with another.
  int width_;
  int height_;
  std::unique_ptr<TransportDIB> dib_;

  // When the device is mapped, this is the image. Null when umapped.
  std::unique_ptr<SkCanvas> mapped_canvas_;
};

class ImageDataSimpleBackend : public PPB_ImageData_Impl::Backend {
 public:
  ImageDataSimpleBackend();

  ImageDataSimpleBackend(const ImageDataSimpleBackend&) = delete;
  ImageDataSimpleBackend& operator=(const ImageDataSimpleBackend&) = delete;

  ~ImageDataSimpleBackend() override;

  // PPB_ImageData_Impl::Backend implementation.
  bool Init(PPB_ImageData_Impl* impl,
            PP_ImageDataFormat format,
            int width,
            int height,
            bool init_to_zero) override;
  bool IsMapped() const override;
  TransportDIB* GetTransportDIB() const override;
  void* Map() override;
  void Unmap() override;
  int32_t GetSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion** region) override;
  SkCanvas* GetCanvas() override;
  SkBitmap GetMappedBitmap() const override;

 private:
  base::UnsafeSharedMemoryRegion shm_region_;
  base::WritableSharedMemoryMapping shm_mapping_;
  // skia_bitmap_ is backed by shared_memory_.
  SkBitmap skia_bitmap_;
  std::unique_ptr<SkCanvas> skia_canvas_;
  uint32_t map_count_;
};

// Manages mapping an image resource if necessary. Use this to ensure the
// image is mapped. The destructor will put the image back into the previous
// state. You must check is_valid() to make sure the image was successfully
// mapped before using it.
//
// Example:
//   ImageDataAutoMapper mapper(image_data);
//   if (!mapper.is_valid())
//     return utter_failure;
//   image_data->mapped_canvas()->blah();  // Guaranteed valid.
class ImageDataAutoMapper {
 public:
  explicit ImageDataAutoMapper(PPB_ImageData_Impl* image_data)
      : image_data_(image_data) {
    if (image_data_->IsMapped()) {
      is_valid_ = true;
      needs_unmap_ = false;
    } else {
      is_valid_ = needs_unmap_ = !!image_data_->Map();
    }
  }

  ImageDataAutoMapper(const ImageDataAutoMapper&) = delete;
  ImageDataAutoMapper& operator=(const ImageDataAutoMapper&) = delete;

  ~ImageDataAutoMapper() {
    if (needs_unmap_)
      image_data_->Unmap();
  }

  // Check this to see if the image was successfully mapped. If this is false,
  // the image could not be mapped and is unusable.
  bool is_valid() const { return is_valid_; }

 private:
  raw_ptr<PPB_ImageData_Impl> image_data_;
  bool is_valid_;
  bool needs_unmap_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_IMAGE_DATA_IMPL_H_
