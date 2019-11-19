/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_info.h"
#include "third_party/blink/renderer/core/loader/resource/multipart_image_resource_parser.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class FetchParameters;
class ImageResourceContent;
class ResourceClient;
class ResourceFetcher;

// ImageResource implements blink::Resource interface and image-specific logic
// for loading images.
// Image-related things (blink::Image and ImageResourceObserver) are handled by
// ImageResourceContent.
// Most users should use ImageResourceContent instead of ImageResource.
// https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit?usp=sharing
//
// As for the lifetimes of ImageResourceContent::m_image and m_data, see this
// document:
// https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
class CORE_EXPORT ImageResource final
    : public Resource,
      public MultipartImageResourceParser::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResource);

 public:
  // Use ImageResourceContent::Fetch() unless ImageResource is required.
  // TODO(hiroshige): Make Fetch() private.
  static ImageResource* Fetch(FetchParameters&, ResourceFetcher*);

  // TODO(hiroshige): Make Create() test-only by refactoring ImageDocument.
  static ImageResource* Create(const ResourceRequest&);
  static ImageResource* CreateForTest(const KURL&);

  ImageResource(const ResourceRequest&,
                const ResourceLoaderOptions&,
                ImageResourceContent*,
                bool is_placeholder);
  ~ImageResource() override;

  ImageResourceContent* GetContent();
  const ImageResourceContent* GetContent() const;

  void ReloadIfLoFiOrPlaceholderImage(ResourceFetcher*,
                                      ReloadLoFiOrPlaceholderPolicy) override;

  void DidAddClient(ResourceClient*) override;

  ResourcePriority PriorityFromObservers() override;

  void AllClientsAndObserversRemoved() override;

  MatchStatus CanReuse(const FetchParameters&) const override;
  bool CanUseCacheValidator() const override;

  scoped_refptr<const SharedBuffer> ResourceBuffer() const override;
  void NotifyStartLoad() override;
  void ResponseReceived(const ResourceResponse&) override;
  void AppendData(const char*, size_t) override;
  void Finish(base::TimeTicks finish_time,
              base::SingleThreadTaskRunner*) override;
  void FinishAsError(const ResourceError&,
                     base::SingleThreadTaskRunner*) override;

  // For compatibility, images keep loading even if there are HTTP errors.
  bool ShouldIgnoreHTTPStatusCodeErrors() const override { return true; }

  // MultipartImageResourceParser::Client
  void OnePartInMultipartReceived(const ResourceResponse&) final;
  void MultipartDataReceived(const char*, size_t) final;

  bool ShouldShowPlaceholder() const;
  bool ShouldShowLazyImagePlaceholder() const;

  // If the ImageResource came from a user agent CSS stylesheet then we should
  // flag it so that it can persist beyond navigation.
  void FlagAsUserAgentResource();

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

  void Trace(blink::Visitor*) override;

 private:
  enum class MultipartParsingState : uint8_t {
    kWaitingForFirstPart,
    kParsingFirstPart,
    kFinishedParsingFirstPart,
  };

  class ImageResourceInfoImpl;
  class ImageResourceFactory;

  // Only for ImageResourceInfoImpl.
  void DecodeError(bool all_data_received);
  bool IsAccessAllowed(
      ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin) const;

  bool HasClientsOrObservers() const override;

  void UpdateImageAndClearBuffer();
  void UpdateImage(scoped_refptr<SharedBuffer>,
                   ImageResourceContent::UpdateImageOption,
                   bool all_data_received);

  void NotifyFinished() override;

  void DestroyDecodedDataIfPossible() override;
  void DestroyDecodedDataForFailedRevalidation() override;

  void FlushImageIfNeeded();

  bool ShouldReloadBrokenPlaceholder() const;

  Member<ImageResourceContent> content_;

  Member<MultipartImageResourceParser> multipart_parser_;
  MultipartParsingState multipart_parsing_state_ =
      MultipartParsingState::kWaitingForFirstPart;

  // Indicates if the ImageResource is currently scheduling a reload, e.g.
  // because reloadIfLoFi() was called.
  bool is_scheduling_reload_;

  // Indicates if this ImageResource is either attempting to load a placeholder
  // image, or is a (possibly broken) placeholder image.
  enum class PlaceholderOption {
    // Do not show or reload placeholder.
    kDoNotReloadPlaceholder,

    // Show placeholder, and do not reload. The original image will still be
    // loaded and shown if the image is explicitly reloaded, e.g. when
    // ReloadIfLoFiOrPlaceholderImage is called with kReloadAlways.
    kShowAndDoNotReloadPlaceholder,

    // Do not show placeholder, reload only when decode error occurs.
    kReloadPlaceholderOnDecodeError,

    // Show placeholder and reload.
    kShowAndReloadPlaceholderAlways,
  };
  PlaceholderOption placeholder_option_;

  base::TimeTicks last_flush_time_;

  bool is_during_finish_as_error_ = false;

  bool is_referenced_from_ua_stylesheet_ = false;

  bool is_pending_flushing_ = false;
};

DEFINE_RESOURCE_TYPE_CASTS(Image);

}  // namespace blink

#endif
