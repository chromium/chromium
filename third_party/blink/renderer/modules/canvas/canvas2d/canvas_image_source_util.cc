// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"

#include "base/check.h"
#include "base/memory/scoped_refptr.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2052)
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_image_value.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

CanvasImageSource* ToCanvasImageSource(const V8CanvasImageSource* value,
                                       ExceptionState& exception_state) {
  DCHECK(value);

  switch (value->GetContentType()) {
    case V8CanvasImageSource::ContentType::kCSSImageValue:
      return value->GetAsCSSImageValue();
    case V8CanvasImageSource::ContentType::kHTMLCanvasElement: {
      if (value->GetAsHTMLCanvasElement()->Size().IsEmpty()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The image argument is a canvas element with a width "
            "or height of 0.");
        return nullptr;
      }
      return value->GetAsHTMLCanvasElement();
    }
    case V8CanvasImageSource::ContentType::kHTMLImageElement:
      return value->GetAsHTMLImageElement();
    case V8CanvasImageSource::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = value->GetAsHTMLVideoElement();
      video->VideoWillBeDrawnToCanvas();
      return video;
    }
    case V8CanvasImageSource::ContentType::kImageBitmap:
      if (value->GetAsImageBitmap()->IsNeutered()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The image source is detached");
        return nullptr;
      }
      return value->GetAsImageBitmap();
    case V8CanvasImageSource::ContentType::kOffscreenCanvas:
      if (value->GetAsOffscreenCanvas()->IsNeutered()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The image source is detached");
        return nullptr;
      }
      if (value->GetAsOffscreenCanvas()->Size().IsEmpty()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The image argument is an OffscreenCanvas element "
            "with a width or height of 0.");
        return nullptr;
      }
      return value->GetAsOffscreenCanvas();
    case V8CanvasImageSource::ContentType::kSVGImageElement:
      return value->GetAsSVGImageElement();
    case V8CanvasImageSource::ContentType::kVideoFrame: {
      VideoFrame* video_frame = value->GetAsVideoFrame();
      if (!video_frame->frame()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The VideoFrame has been closed");
        return nullptr;
      }
      return video_frame;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool WouldTaintCanvasOrigin(CanvasImageSource* image_source) {
  // Don't taint the canvas on data URLs. This special case is needed here
  // because CanvasImageSource::WouldTaintOrigin() can return false for data
  // URLs due to restrictions on SVG foreignObject nodes as described in
  // https://crbug.com/294129.
  // TODO(crbug.com/294129): Remove the restriction on foreignObject nodes, then
  // this logic isn't needed, CanvasImageSource::SourceURL() isn't needed, and
  // this function can just be image_source->WouldTaintOrigin().
  const KURL& source_url = image_source->SourceURL();
  const bool has_url = (source_url.IsValid() && !source_url.IsAboutBlankURL());
  if (has_url && source_url.ProtocolIsData()) {
    return false;
  }

  return image_source->WouldTaintOrigin();
}

}  // namespace blink
