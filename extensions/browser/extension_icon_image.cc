// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_image.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"

// The ImageSkia provided by extensions::IconImage contains ImageSkiaReps that
// are computed and updated using the following algorithm (if no default icon
// was supplied, transparent icon is considered the default):
// - |LoadImageForScale()| searches the extension for an icon of an
//   appropriate size. If the extension doesn't have a icon resource needed for
//   the image representation, the default icon's representation for the
//   requested scale factor is returned by ImageSkiaSource.
// - If the extension has the resource, IconImage tries to load it using
//   ImageLoader.
// - |ImageLoader| is asynchronous.
//  - ImageSkiaSource will initially return transparent image resource of the
//    desired size.
//  - The image will be updated with an appropriate image representation when
//    the |ImageLoader| finishes. The image representation is chosen the same
//    way as in the synchronous case. The observer is notified of the image
//    change, unless the added image representation is transparent (in which
//    case the image had already contained the appropriate image
//    representation).

namespace {

extensions::ExtensionResource GetExtensionIconResource(
    const extensions::Extension& extension,
    const ExtensionIconSet& icons,
    int size,
    ExtensionIconSet::MatchType match_type) {
  const std::string& path = icons.Get(size, match_type);
  return path.empty() ? extensions::ExtensionResource()
                      : extension.GetResource(path);
}

class BlankImageSource : public gfx::CanvasImageSource {
 public:
  explicit BlankImageSource(const gfx::Size& size_in_dip)
      : CanvasImageSource(size_in_dip) {}
  ~BlankImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawColor(SkColorSetARGB(0, 0, 0, 0));
  }

  DISALLOW_COPY_AND_ASSIGN(BlankImageSource);
};

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// IconImage::Source

class IconImage::Source : public gfx::ImageSkiaSource {
 public:
  Source(IconImage* host, const gfx::Size& size_in_dip);
  ~Source() override;

  void ResetHost();

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images, possibly asynchronously. NULLed out when the IconImage
  // is destroyed.
  IconImage* host_;

  // Image whose representations will be used until |host_| loads the real
  // representations for the image.
  gfx::ImageSkia blank_image_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

IconImage::Source::Source(IconImage* host, const gfx::Size& size_in_dip)
    : host_(host),
      blank_image_(std::make_unique<BlankImageSource>(size_in_dip),
                   size_in_dip) {}

IconImage::Source::~Source() {
}

void IconImage::Source::ResetHost() {
  host_ = NULL;
}

gfx::ImageSkiaRep IconImage::Source::GetImageForScale(float scale) {
  if (host_)
    host_->LoadImageForScaleAsync(scale);
  return blank_image_.GetRepresentation(scale);
}

////////////////////////////////////////////////////////////////////////////////
// IconImage

IconImage::IconImage(content::BrowserContext* context,
                     const Extension* extension,
                     const ExtensionIconSet& icon_set,
                     int resource_size_in_dip,
                     bool keep_original_size,
                     const gfx::ImageSkia& default_icon,
                     Observer* observer)
    : browser_context_(context),
      extension_(extension),
      icon_set_(icon_set),
      resource_size_in_dip_(resource_size_in_dip),
      keep_original_size_(keep_original_size),
      did_complete_initial_load_(false),
      source_(NULL),
      default_icon_(gfx::ImageSkiaOperations::CreateResizedImage(
          default_icon,
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(resource_size_in_dip, resource_size_in_dip))) {
  if (observer)
    AddObserver(observer);
  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  source_ = new Source(this, resource_size);
  image_skia_ = gfx::ImageSkia(base::WrapUnique(source_), resource_size);
  image_ = gfx::Image(image_skia_);

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_REMOVED,
                 content::NotificationService::AllSources());
}

IconImage::IconImage(content::BrowserContext* context,
                     const Extension* extension,
                     const ExtensionIconSet& icon_set,
                     int resource_size_in_dip,
                     const gfx::ImageSkia& default_icon,
                     Observer* observer)
    : IconImage(context,
                extension,
                icon_set,
                resource_size_in_dip,
                /* keep_original_size */ false,
                default_icon,
                observer) {}

void IconImage::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IconImage::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

IconImage::~IconImage() {
  for (auto& observer : observers_)
    observer.OnExtensionIconImageDestroyed(this);
  source_->ResetHost();
}

void IconImage::LoadImageForScaleAsync(float scale) {
  // Do nothing if extension is unloaded.
  if (!extension_)
    return;

  const int resource_size_in_pixel =
      static_cast<int>(resource_size_in_dip_ * scale);

  extensions::ExtensionResource resource;

  // Find extension resource for non bundled component extensions.
  resource =
      GetExtensionIconResource(*extension_, icon_set_, resource_size_in_pixel,
                               ExtensionIconSet::MATCH_BIGGER);

  // If resource is not found by now, try matching smaller one.
  if (resource.empty()) {
    resource =
        GetExtensionIconResource(*extension_, icon_set_, resource_size_in_pixel,
                                 ExtensionIconSet::MATCH_SMALLER);
  }

  if (!resource.empty()) {
    std::vector<ImageLoader::ImageRepresentation> info_list;
    const ImageLoader::ImageRepresentation::ResizeCondition resize_condition =
        keep_original_size_ ? ImageLoader::ImageRepresentation::NEVER_RESIZE
                            : ImageLoader::ImageRepresentation::ALWAYS_RESIZE;
    info_list.push_back(ImageLoader::ImageRepresentation(
        resource, resize_condition,
        gfx::Size(resource_size_in_pixel, resource_size_in_pixel), scale));

    extensions::ImageLoader* loader =
        extensions::ImageLoader::Get(browser_context_);
    loader->LoadImagesAsync(
        extension_.get(), info_list,
        base::BindOnce(&IconImage::OnImageLoaded,
                       weak_ptr_factory_.GetWeakPtr(), scale));
  } else {
    // If there is no resource found, update from the default icon.
    const gfx::ImageSkiaRep& rep = default_icon_.GetRepresentation(scale);
    if (!rep.is_null()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&IconImage::OnImageRepLoaded,
                                    weak_ptr_factory_.GetWeakPtr(), rep));
    }
  }
}

void IconImage::OnImageLoaded(float scale, const gfx::Image& image_in) {
  const gfx::ImageSkia* image =
      image_in.IsEmpty() ? &default_icon_ : image_in.ToImageSkia();

  // Maybe default icon was not set.
  if (image->isNull())
    return;

  OnImageRepLoaded(image->GetRepresentation(scale));
}

void IconImage::OnImageRepLoaded(const gfx::ImageSkiaRep& rep) {
  DCHECK(!rep.is_null());
  did_complete_initial_load_ = true;

  image_skia_.RemoveRepresentation(rep.scale());
  image_skia_.AddRepresentation(rep);
  image_skia_.RemoveUnsupportedRepresentationsForScale(rep.scale());

  // Update the image to use the updated image skia.
  // It's a shame we have to do this because it means that all the other
  // representations stored on |image_| will be deleted, but unfortunately
  // there's no way to combine the storage of two images.
  image_ = gfx::Image(image_skia_);

  for (auto& observer : observers_)
    observer.OnExtensionIconImageChanged(this);
}

void IconImage::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_REMOVED);

  const Extension* extension = content::Details<const Extension>(details).ptr();

  if (extension_.get() == extension)
    extension_ = nullptr;
}

}  // namespace extensions
