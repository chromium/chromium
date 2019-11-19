// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_IMAGE_LOADER_H_
#define EXTENSIONS_BROWSER_IMAGE_LOADER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_resource.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
class ImageFamily;
}

namespace extensions {

class Extension;

using ImageLoaderImageCallback = base::OnceCallback<void(const gfx::Image&)>;
using ImageLoaderImageFamilyCallback =
    base::OnceCallback<void(gfx::ImageFamily)>;

// This class is responsible for asynchronously loading extension images and
// calling a callback when an image is loaded.
// The views need to load their icons asynchronously might be deleted before
// the images have loaded. If you pass your callback using a weak_ptr, this
// will make sure the callback won't be called after the view is deleted.
class ImageLoader : public KeyedService {
 public:
  // Information about a singe image representation to load from an extension
  // resource.
  struct ImageRepresentation {
    // Enum values to indicate whether to resize loaded bitmap when it is larger
    // than |desired_size| or always resize it.
    enum ResizeCondition { RESIZE_WHEN_LARGER, ALWAYS_RESIZE, NEVER_RESIZE };

    ImageRepresentation(const ExtensionResource& resource,
                        ResizeCondition resize_condition,
                        const gfx::Size& desired_size,
                        float scale_factor);
    ~ImageRepresentation();

    // Extension resource to load.
    ExtensionResource resource;

    ResizeCondition resize_condition;

    // When |resize_method| is ALWAYS_RESIZE or when the loaded image is larger
    // than |desired_size| it will be resized to these dimensions.
    gfx::Size desired_size;

    // |scale_factor| is used to construct the loaded gfx::ImageSkia.
    float scale_factor;
  };

  struct LoadResult;

  // Returns the instance for the given |context| or NULL if none. This is
  // a convenience wrapper around ImageLoaderFactory::GetForBrowserContext.
  static ImageLoader* Get(content::BrowserContext* context);

  ImageLoader();
  ~ImageLoader() override;

  // Specify image resource to load. If the loaded image is larger than
  // |max_size| it will be resized to those dimensions. IMPORTANT NOTE: this
  // function may call back your callback synchronously (ie before it returns)
  // if the image was found in the cache.
  // Note this method loads a raw bitmap from the resource. All sizes given are
  // assumed to be in pixels.
  // TODO(estade): remove this in favor of LoadImageAtEveryScaleFactorAsync,
  // and rename the latter to LoadImageAsync.
  void LoadImageAsync(const Extension* extension,
                      const ExtensionResource& resource,
                      const gfx::Size& max_size,
                      ImageLoaderImageCallback callback);

  // Loads a gfx::Image that has representations at all scale factors we are
  // likely to care about. That includes every scale for which we pack resources
  // in ResourceBundle plus the scale for all currently attached displays. The
  // image is returned via |callback|.
  void LoadImageAtEveryScaleFactorAsync(const Extension* extension,
                                        const gfx::Size& dip_size,
                                        ImageLoaderImageCallback callback);

  // Same as LoadImageAsync() above except it loads multiple images from the
  // same extension. This is used to load multiple resolutions of the same image
  // type.
  void LoadImagesAsync(const Extension* extension,
                       const std::vector<ImageRepresentation>& info_list,
                       ImageLoaderImageCallback callback);

  // Same as LoadImagesAsync() above except it loads into an image family. This
  // is used to load multiple images of different logical sizes as opposed to
  // LoadImagesAsync() which loads different scale factors of the same logical
  // image size.
  //
  // If multiple images of the same logical size are loaded, they will be
  // combined into a single ImageSkia in the ImageFamily.
  void LoadImageFamilyAsync(const Extension* extension,
                            const std::vector<ImageRepresentation>& info_list,
                            ImageLoaderImageFamilyCallback callback);

 private:
  void ReplyBack(ImageLoaderImageCallback callback,
                 const std::vector<LoadResult>& load_result);

  void ReplyBackWithImageFamily(ImageLoaderImageFamilyCallback callback,
                                const std::vector<LoadResult>& load_result);

  base::WeakPtrFactory<ImageLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IMAGE_LOADER_H_
