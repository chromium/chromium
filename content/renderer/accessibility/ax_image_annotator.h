// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_

#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace blink {

class WebAXObject;

}  // namespace blink

namespace content {

class ContentClient;

// This class gets notified that certain images have been added, removed or
// updated on a page. This class is then responsible for retrieving the
// automatic label for all images and notifying the RenderAccessibility that
// owns it to update the relevant image annotations.
class CONTENT_EXPORT AXImageAnnotator : public base::CheckedObserver {
 public:
  AXImageAnnotator(
      RenderAccessibilityImpl* const render_accessibility,
      const std::string& preferred_language,
      mojo::PendingRemote<image_annotation::mojom::Annotator> annotator);
  ~AXImageAnnotator() override;

  void Destroy();

  std::string GetImageAnnotation(blink::WebAXObject& image) const;
  ax::mojom::ImageAnnotationStatus GetImageAnnotationStatus(
      blink::WebAXObject& image) const;
  bool HasAnnotationInCache(blink::WebAXObject& image) const;
  bool HasImageInCache(const blink::WebAXObject& image) const;

  void OnImageAdded(blink::WebAXObject& image);
  void OnImageUpdated(blink::WebAXObject& image);
  void OnImageRemoved(blink::WebAXObject& image);

  void set_preferred_language(const std::string& language) {
    preferred_language_ = language;
  }

 private:
  // Keeps track of the image data and the automatic annotation for each image.
  class ImageInfo final {
   public:
    ImageInfo(const blink::WebAXObject& image);
    virtual ~ImageInfo();

    mojo::PendingRemote<image_annotation::mojom::ImageProcessor>
    GetImageProcessor();
    bool HasAnnotation() const;

    ax::mojom::ImageAnnotationStatus status() const { return status_; }

    void set_status(ax::mojom::ImageAnnotationStatus status) {
      DCHECK_NE(status, ax::mojom::ImageAnnotationStatus::kNone);
      status_ = status;
    }

    std::string annotation() const {
      return annotation_.value_or("");
    }

    void set_annotation(std::string annotation) { annotation_ = annotation; }

   private:
    image_annotation::ImageProcessor image_processor_;
    ax::mojom::ImageAnnotationStatus status_;
    base::Optional<std::string> annotation_;
  };

  // Retrieves the image data from the renderer.
  static SkBitmap GetImageData(const blink::WebAXObject& image);

  // Used by tests to override the content client.
  virtual ContentClient* GetContentClient() const;

  // Given a WebImage, it uses the URL of the main document and the src
  // attribute of the image, to generate a unique identifier for the image that
  // could be provided to the image annotation service.
  //
  // This method is virtual to allow overriding it from tests.
  virtual std::string GenerateImageSourceId(
      const blink::WebAXObject& image) const;

  // Removes the automatic image annotations from all images.
  void MarkAllImagesDirty();

  // Marks a node in the accessibility tree dirty when an image annotation
  // changes. Also marks dirty a link or document that immediately contains
  // an image.
  void MarkDirty(const blink::WebAXObject& image) const;

  // Gets called when an image gets annotated by the image annotation service.
  void OnImageAnnotated(const blink::WebAXObject& image,
                        image_annotation::mojom::AnnotateImageResultPtr result);

  // Only for local logging when running with --v=1.
  std::string GetDocumentUrl() const;

  // Weak, owns us.
  RenderAccessibilityImpl* const render_accessibility_;

  // The language in which to request image descriptions.
  std::string preferred_language_;

  // A pointer to the automatic image annotation service.
  mojo::Remote<image_annotation::mojom::Annotator> annotator_;

  // Keeps track of the image data and the automatic annotations for each image.
  //
  // The key is retrieved using WebAXObject::AxID().
  std::unordered_map<int, ImageInfo> image_annotations_;

  // This member needs to be last because it should destructed first.
  base::WeakPtrFactory<AXImageAnnotator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AXImageAnnotator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_IMAGE_ANNOTATOR_H_
