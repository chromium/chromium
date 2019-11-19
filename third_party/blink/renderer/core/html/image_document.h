/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMAGE_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMAGE_DOCUMENT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"

namespace blink {

class ImageResource;

class CORE_EXPORT ImageDocument final : public HTMLDocument {
 public:
  explicit ImageDocument(const DocumentInit&);

  ImageResourceContent* CachedImage();

  // TODO(hiroshige): Remove this.
  ImageResource* CachedImageResourceDeprecated();

  HTMLImageElement* ImageElement() const { return image_element_.Get(); }
  IntSize ImageSize() const;

  void WindowSizeChanged();
  void ImageUpdated();
  void ImageClicked(int x, int y);
  void ImageLoaded();
  void UpdateImageStyle();
  bool ShouldShrinkToFit() const;

  void Trace(Visitor*) override;

 private:
  DocumentParser* CreateParser() override;

  void CreateDocumentStructure();

  // Calculates how large the div needs to be to properly center the image.
  int CalculateDivWidth();

  // These methods are for shrink_to_fit_mode_ == kDesktop.
  void ResizeImageToFit();
  void RestoreImageSize();
  bool ImageFitsInWindow() const;
  // Calculates the image size multiplier that's needed to fit the image to
  // the window, taking into account page zoom and device scale.
  float Scale() const;

  Member<HTMLDivElement> div_element_;
  Member<HTMLImageElement> image_element_;

  // Whether enough of the image has been loaded to determine its size
  bool image_size_is_known_;

  // Whether the image is shrunk to fit or not
  bool did_shrink_image_;

  // Whether the image should be shrunk or not
  bool should_shrink_image_;

  // Whether the image has finished loading or not
  bool image_is_loaded_;

  // Desktop: State of the mouse cursor in the image style
  enum MouseCursorMode { kDefault, kZoomIn, kZoomOut };
  MouseCursorMode style_mouse_cursor_mode_;

  enum ShrinkToFitMode { kViewport, kDesktop };
  ShrinkToFitMode shrink_to_fit_mode_;

  FRIEND_TEST_ALL_PREFIXES(ImageDocumentViewportTest, ZoomForDSFScaleImage);
  FRIEND_TEST_ALL_PREFIXES(ImageDocumentViewportTest, DivWidthWithZoomForDSF);
};

DEFINE_DOCUMENT_TYPE_CASTS(ImageDocument);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMAGE_DOCUMENT_H_
