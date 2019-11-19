// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_CONTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_CONTENT_H_

#include <memory>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_status.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class FetchParameters;
class ImageResourceInfo;
class ImageResourceObserver;
class ResourceError;
class ResourceFetcher;
class ResourceResponse;
class SecurityContext;

// ImageResourceContent is a container that holds fetch result of
// an ImageResource in a decoded form.
// Classes that use the fetched images
// should hold onto this class and/or inherit ImageResourceObserver,
// instead of holding onto ImageResource or inheriting ResourceClient.
// https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit?usp=sharing
// TODO(hiroshige): Make ImageResourceContent ResourceClient and remove the
// word 'observer' from ImageResource.
// TODO(hiroshige): Rename local variables of type ImageResourceContent to
// e.g. |imageContent|. Currently they have Resource-like names.
class CORE_EXPORT ImageResourceContent final
    : public GarbageCollected<ImageResourceContent>,
      public ImageObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResourceContent);

 public:
  // Used for loading.
  // Returned content will be associated immediately later with ImageResource.
  static ImageResourceContent* CreateNotStarted() {
    return MakeGarbageCollected<ImageResourceContent>(nullptr);
  }

  // Creates ImageResourceContent from an already loaded image.
  static ImageResourceContent* CreateLoaded(scoped_refptr<blink::Image>);

  static ImageResourceContent* CreateLazyImagePlaceholder();

  static ImageResourceContent* Fetch(FetchParameters&, ResourceFetcher*);

  explicit ImageResourceContent(scoped_refptr<blink::Image> = nullptr);

  // Returns the NullImage() if the image is not available yet.
  blink::Image* GetImage() const;
  bool HasImage() const { return image_.get(); }

  // The device pixel ratio we got from the server for this image, or 1.0.
  float DevicePixelRatioHeaderValue() const;
  bool HasDevicePixelRatioHeaderValue() const;

  // Returns the intrinsic width and height of the image, or 0x0 if no image
  // exists. If the image is a BitmapImage, then this corresponds to the
  // physical pixel dimensions of the image. If the image is an SVGImage, this
  // does not quite return the intrinsic width/height, but rather a concrete
  // object size resolved using a default object size of 300x150.
  // TODO(fs): Make SVGImages return proper intrinsic width/height.
  IntSize IntrinsicSize(
      RespectImageOrientationEnum should_respect_image_orientation) const;

  void AddObserver(ImageResourceObserver*);
  void RemoveObserver(ImageResourceObserver*);

  bool IsSizeAvailable() const {
    return size_available_ != Image::kSizeUnavailable;
  }

  void Trace(blink::Visitor*) override;

  // Content status and deriving predicates.
  // https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit#heading=h.6cyqmir0f30h
  // Normal transitions:
  //   kNotStarted -> kPending -> kCached|kLoadError|kDecodeError.
  // Additional transitions in multipart images:
  //   kCached -> kLoadError|kDecodeError.
  // Transitions due to revalidation:
  //   kCached -> kPending.
  // Transitions due to reload:
  //   kCached|kLoadError|kDecodeError -> kPending.
  //
  // ImageResourceContent::GetContentStatus() can be different from
  // ImageResource::GetStatus(). Use ImageResourceContent::GetContentStatus().
  ResourceStatus GetContentStatus() const;
  bool IsLoaded() const;
  bool IsLoading() const;
  bool ErrorOccurred() const;
  bool LoadFailedOrCanceled() const;

  // Redirecting methods to Resource.
  const KURL& Url() const;
  base::TimeTicks LoadResponseEnd() const;
  bool IsAccessAllowed();
  const ResourceResponse& GetResponse() const;
  base::Optional<ResourceError> GetResourceError() const;
  // DEPRECATED: ImageResourceContents consumers shouldn't need to worry about
  // whether the underlying Resource is being revalidated.
  bool IsCacheValidator() const;

  // For FrameSerializer.
  bool HasCacheControlNoStoreHeader() const;

  void EmulateLoadStartedForInspector(ResourceFetcher*,
                                      const KURL&,
                                      const AtomicString& initiator_name);

  void SetNotRefetchableDataFromDiskCache() {
    is_refetchable_data_from_disk_cache_ = false;
  }

  // The following public methods should be called from ImageResource only.

  // UpdateImage() is the single control point of image content modification
  // from ImageResource that all image updates should call.
  // We clear and/or update images in this single method
  // (controlled by UpdateImageOption) rather than providing separate methods,
  // in order to centralize state changes and
  // not to expose the state in between to ImageResource.
  enum UpdateImageOption {
    // Updates the image (including placeholder and decode error handling
    // and notifying observers) if needed.
    kUpdateImage,

    // Clears the image and then updates the image if needed.
    kClearAndUpdateImage,

    // Clears the image and always notifies observers (without updating).
    kClearImageAndNotifyObservers,
  };
  enum class UpdateImageResult {
    kNoDecodeError,

    // Decode error occurred. Observers are not notified.
    // Only occurs when UpdateImage or ClearAndUpdateImage is specified.
    kShouldDecodeError,
  };
  WARN_UNUSED_RESULT UpdateImageResult UpdateImage(scoped_refptr<SharedBuffer>,
                                                   ResourceStatus,
                                                   UpdateImageOption,
                                                   bool all_data_received,
                                                   bool is_multipart);

  void NotifyStartLoad();
  void DestroyDecodedData();
  void DoResetAnimation();

  void SetImageResourceInfo(ImageResourceInfo*);

  ResourcePriority PriorityFromObservers() const;
  scoped_refptr<const SharedBuffer> ResourceBuffer() const;
  bool ShouldUpdateImageImmediately() const;
  bool HasObservers() const {
    return !observers_.IsEmpty() || !finished_observers_.IsEmpty();
  }
  bool IsRefetchableDataFromDiskCache() const {
    return is_refetchable_data_from_disk_cache_;
  }

  ImageDecoder::CompressionFormat GetCompressionFormat() const;

  // Returns true if the image content is well-compressed (and not full of
  // extraneous metadata). "well-compressed" is determined by comparing the
  // image's compression ratio against a specific value that is defined by an
  // unoptimized image feature policy on |context|.
  bool IsAcceptableCompressionRatio(const SecurityContext& context);

  void LoadDeferredImage(ResourceFetcher* fetcher);

 private:
  using CanDeferInvalidation = ImageResourceObserver::CanDeferInvalidation;

  // ImageObserver
  void DecodedSizeChangedTo(const blink::Image*, size_t new_size) override;
  bool ShouldPauseAnimation(const blink::Image*) override;
  void Changed(const blink::Image*) override;
  void AsyncLoadCompleted(const blink::Image*) override;

  scoped_refptr<Image> CreateImage(bool is_multipart);
  void ClearImage();

  enum NotifyFinishOption { kShouldNotifyFinish, kDoNotNotifyFinish };

  void NotifyObservers(NotifyFinishOption, CanDeferInvalidation);
  void HandleObserverFinished(ImageResourceObserver*);
  void UpdateToLoadedContentStatus(ResourceStatus);
  void UpdateImageAnimationPolicy();

  class ProhibitAddRemoveObserverInScope : public base::AutoReset<bool> {
   public:
    ProhibitAddRemoveObserverInScope(const ImageResourceContent* content)
        : AutoReset(&content->is_add_remove_observer_prohibited_, true) {}
  };

  ResourceStatus content_status_ = ResourceStatus::kNotStarted;

  // Indicates if this resource's encoded image data can be purged and refetched
  // from disk cache to save memory usage. See crbug/664437.
  bool is_refetchable_data_from_disk_cache_;

  mutable bool is_add_remove_observer_prohibited_ = false;

  Image::SizeAvailability size_available_ = Image::kSizeUnavailable;

  Member<ImageResourceInfo> info_;

  float device_pixel_ratio_header_value_;
  bool has_device_pixel_ratio_header_value_;

  scoped_refptr<blink::Image> image_;

  HashCountedSet<ImageResourceObserver*> observers_;
  HashCountedSet<ImageResourceObserver*> finished_observers_;

#if DCHECK_IS_ON()
  bool is_update_image_being_called_ = false;
#endif
};

}  // namespace blink

#endif
