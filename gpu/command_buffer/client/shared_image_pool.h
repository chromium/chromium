#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_POOL_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_POOL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class SharedImageInterface;

// Structure holding the necessary information to create shared images and
// describe its characteristics in the SharedImagePool. It will be constant for
// all the shared images in the pool.
struct GPU_EXPORT ImageInfo {
  gfx::Size size;
  viz::SharedImageFormat format;
  SharedImageUsageSet usage;
  gfx::ColorSpace color_space;
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;

  ImageInfo(gfx::Size size,
            viz::SharedImageFormat format,
            SharedImageUsageSet usage)
      : size(size), format(format), usage(usage) {}

  ImageInfo(gfx::Size size,
            viz::SharedImageFormat format,
            SharedImageUsageSet usage,
            gfx::ColorSpace color_space,
            GrSurfaceOrigin surface_origin,
            SkAlphaType alpha_type)
      : size(size),
        format(format),
        usage(usage),
        color_space(color_space),
        surface_origin(surface_origin),
        alpha_type(alpha_type) {}

  bool operator==(const ImageInfo& other) const {
    return size == other.size && format == other.format &&
           usage == other.usage && color_space == other.color_space &&
           surface_origin == other.surface_origin &&
           alpha_type == other.alpha_type;
  }
};

// A reference-counted image class that wraps a GPU ClientSharedImage. Clients
// can optionally extend this class to add its own metadata and logic in
// addition to the shared image it wraps. This allow clients to create its own
// custom pool of images of ClientImage type and are not limited to creating
// pool of only ClientSharedImage. See unittests for example.
class GPU_EXPORT ClientImage : public base::RefCounted<ClientImage> {
 public:
  explicit ClientImage(scoped_refptr<ClientSharedImage> shared_image);

  // Returns the reference on the underlying shared image. Note that clients
  // using it should ensure that the returned reference does not outlive the
  // ClientImage.
  const scoped_refptr<ClientSharedImage>& GetSharedImage() const;

  // Returns a sync token which should be waited upon before using this image.
  const SyncToken& GetSyncToken() const;

  // Sets the sync token which will be waited upon before releasing this image
  // for re-use or destruction.
  void SetReleaseSyncToken(SyncToken release_sync_token);

 protected:
  friend class base::RefCounted<ClientImage>;
  friend class SharedImagePoolBase;
  virtual ~ClientImage();

 private:
  scoped_refptr<ClientSharedImage> shared_image_;

  // This token has to be waited upon before using/re-using the |shared_image_|.
  // This will be also used internally as a destruction sync token for the
  // shared image.
  SyncToken sync_token_;
};

// This class is designed to handle bulk of functionality of the image pool.
// This also allows to have a template subclass with minimum functionality.
// Since all definitions of templated subclass with be in this header file, we
// want it to be as thin as possible as it will also code generate for all
// possible params and this will increase binary size. Clients will not use this
// class directly.
class GPU_EXPORT SharedImagePoolBase {
 public:
  virtual ~SharedImagePoolBase();

  size_t GetPoolSizeForTesting() const;

 protected:
  SharedImagePoolBase(const ImageInfo& image_info,
                      const scoped_refptr<SharedImageInterface> sii,
                      std::optional<uint8_t> max_pool_size);

  scoped_refptr<ClientSharedImage> CreateSharedImageInternal();
  scoped_refptr<ClientImage> GetImageFromPoolInternal();
  void ReleaseImageInternal(scoped_refptr<ClientImage> image);
  void ClearInternal();
  void ReconfigureInternal(const ImageInfo& image_info);

  // Information used to create new ClientSharedImage.
  ImageInfo image_info_;

  // Interface to the GPU process for creating shared images.
  const scoped_refptr<SharedImageInterface> sii_;

  // Optional maximum size of the pool. It defaults to 0 which means there is no
  // limit on the size of the pool.
  const std::optional<uint8_t> max_pool_size_;

  // Pool of available images.
  std::vector<scoped_refptr<ClientImage>> image_pool_;
};

// Templated class for managing a pool of ClientImageType objects which wraps
// shared images. ClientImageType should be a subclass of ClientImage if
// extended functionality is needed. By default, ClientImageType is ClientImage
// which will result in pool of ClientSharedImage if a client does not need
// additional functionality.
// Clients will use this class and its apis for desired functionality.
template <typename ClientImageType = ClientImage>
class GPU_EXPORT SharedImagePool : public SharedImagePoolBase {
 public:
  static std::unique_ptr<SharedImagePool<ClientImageType>> Create(
      const ImageInfo& image_info,
      const scoped_refptr<SharedImageInterface> sii,
      std::optional<uint8_t> max_pool_size = std::nullopt) {
    CHECK(sii);
    return base::WrapUnique<SharedImagePool<ClientImageType>>(
        new SharedImagePool(image_info, std::move(sii),
                            std::move(max_pool_size)));
  }

  // Clears the pool, deleting all contained images.
  ~SharedImagePool() override = default;

  // Retrieves an image from the pool or creates a new one if the pool is empty.
  scoped_refptr<ClientImageType> GetImage() {
    // Try to get an image from the pool.
    auto image = GetImageFromPoolInternal();
    if (image) {
      return static_cast<ClientImageType*>(image.get());
    }

    // If the pool is empty, create a new image.
    auto shared_image = CreateSharedImageInternal();
    if (!shared_image) {
      LOG(ERROR) << "Unable to create a shared image.";
      return nullptr;
    }
    return base::MakeRefCounted<ClientImageType>(std::move(shared_image));
  }

  // Releases an |image| to the Pool. The |image| will be released/destroyed if
  // the pool is full or will be recycled back into the pool for re-use. Clients
  // can optionally set an release sync token via ::SetReleaseSyncToken() which
  // will be waited upon before releasing or re-using this |image|.
  void ReleaseImage(scoped_refptr<ClientImageType> image) {
    ReleaseImageInternal(std::move(image));
  }

  // Clears the whole pool and destroys all the images.
  void Clear() { ClearInternal(); }

  // Used to reconfigure the pool with new |image_info|. If this info is same as
  // previous one, this operation will be no-op. Note that the max size of the
  // pool will not be reconfigured and remains same.
  void Reconfigure(const ImageInfo& image_info) {
    ReconfigureInternal(image_info);
  }

  // Returns the |image_info_| to the client. Based on this info, clients can
  // decide to continue using this pool or recreate new pool with updated
  // |image_info_|.
  const ImageInfo& GetImageInfo() { return image_info_; }

  // Returns a weak pointer to this pool, allowing for safe reference without
  // ownership.
  base::WeakPtr<SharedImagePool<ClientImageType>> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  SharedImagePool(const ImageInfo& image_info,
                  scoped_refptr<SharedImageInterface> sii,
                  std::optional<uint8_t> max_pool_size)
      : SharedImagePoolBase(image_info,
                            std::move(sii),
                            std::move(max_pool_size)) {}

  base::WeakPtrFactory<SharedImagePool<ClientImageType>> weak_ptr_factory_{
      this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_POOL_H_
