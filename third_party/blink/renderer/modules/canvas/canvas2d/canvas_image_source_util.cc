// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/cssom/css_url_image_value.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

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

  NOTREACHED();
  return nullptr;
}

}  // namespace blink
