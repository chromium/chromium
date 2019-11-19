/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/image_document.h"

#include <limits>

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/raw_data_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ImageEventListener : public NativeEventListener {
 public:
  ImageEventListener(ImageDocument* document) : doc_(document) {}

  bool Matches(const EventListener& other) const override;

  void Invoke(ExecutionContext*, Event*) override;

  void Trace(Visitor* visitor) override {
    visitor->Trace(doc_);
    NativeEventListener::Trace(visitor);
  }

  bool IsImageEventListener() const override { return true; }

 private:
  Member<ImageDocument> doc_;
};

template <>
struct DowncastTraits<ImageEventListener> {
  static bool AllowFrom(const EventListener& event_listener) {
    const NativeEventListener* native_event_listener =
        DynamicTo<NativeEventListener>(event_listener);
    return native_event_listener &&
           native_event_listener->IsImageEventListener();
  }
};

class ImageDocumentParser : public RawDataDocumentParser {
 public:
  ImageDocumentParser(ImageDocument* document)
      : RawDataDocumentParser(document) {}

  ImageDocument* GetDocument() const {
    return ToImageDocument(RawDataDocumentParser::GetDocument());
  }

 private:
  void AppendBytes(const char*, size_t) override;
  void Finish() override;
};

// --------

static String ImageTitle(const String& filename, const IntSize& size) {
  StringBuilder result;
  result.Append(filename);
  result.Append(" (");
  // FIXME: Localize numbers. Safari/OSX shows localized numbers with group
  // separaters. For example, "1,920x1,080".
  result.AppendNumber(size.Width());
  result.Append(static_cast<UChar>(0xD7));  // U+00D7 (multiplication sign)
  result.AppendNumber(size.Height());
  result.Append(')');
  return result.ToString();
}

void ImageDocumentParser::AppendBytes(const char* data, size_t length) {
  if (!length)
    return;

  if (IsDetached())
    return;

  LocalFrame* frame = GetDocument()->GetFrame();
  Settings* settings = frame->GetSettings();
  bool allow_image = !settings || settings->GetImagesEnabled();
  if (auto* client = frame->GetContentSettingsClient())
    allow_image = client->AllowImage(allow_image, GetDocument()->Url());
  if (!allow_image)
    return;

  if (GetDocument()->CachedImageResourceDeprecated()) {
    CHECK_LE(length, std::numeric_limits<unsigned>::max());
    // If decoding has already failed, there's no point in sending additional
    // data to the ImageResource.
    if (GetDocument()->CachedImageResourceDeprecated()->GetStatus() !=
        ResourceStatus::kDecodeError)
      GetDocument()->CachedImageResourceDeprecated()->AppendData(data, length);
  }

  if (!IsDetached())
    GetDocument()->ImageUpdated();
}

void ImageDocumentParser::Finish() {
  if (!IsStopped() && GetDocument()->ImageElement() &&
      GetDocument()->CachedImageResourceDeprecated()) {
    // TODO(hiroshige): Use ImageResourceContent instead of ImageResource.
    ImageResource* cached_image =
        GetDocument()->CachedImageResourceDeprecated();
    DocumentLoader* loader = GetDocument()->Loader();
    cached_image->SetResponse(loader->GetResponse());
    cached_image->Finish(
        loader->GetTiming().ResponseEnd(),
        GetDocument()->GetTaskRunner(TaskType::kInternalLoading).get());

    // Report the natural image size in the page title, regardless of zoom
    // level.  At a zoom level of 1 the image is guaranteed to have an integer
    // size.
    IntSize size = GetDocument()->ImageSize();
    if (size.Width()) {
      // Compute the title, we use the decoded filename of the resource, falling
      // back on the (decoded) hostname if there is no path.
      String file_name =
          DecodeURLEscapeSequences(GetDocument()->Url().LastPathComponent(),
                                   DecodeURLMode::kUTF8OrIsomorphic);
      if (file_name.IsEmpty())
        file_name = GetDocument()->Url().Host();
      GetDocument()->setTitle(ImageTitle(file_name, size));
      if (IsDetached())
        return;
    }

    GetDocument()->ImageUpdated();
    GetDocument()->ImageLoaded();
  }

  if (!IsDetached()) {
    GetDocument()->SetReadyState(Document::kInteractive);
    GetDocument()->FinishedParsing();
  }
}

// --------

ImageDocument::ImageDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, kImageDocumentClass),
      div_element_(nullptr),
      image_element_(nullptr),
      image_size_is_known_(false),
      did_shrink_image_(false),
      should_shrink_image_(ShouldShrinkToFit()),
      image_is_loaded_(false),
      style_mouse_cursor_mode_(kDefault),
      shrink_to_fit_mode_(GetFrame()->GetSettings()->GetViewportEnabled()
                              ? kViewport
                              : kDesktop) {
  SetCompatibilityMode(kQuirksMode);
  LockCompatibilityMode();
}

DocumentParser* ImageDocument::CreateParser() {
  return MakeGarbageCollected<ImageDocumentParser>(this);
}

IntSize ImageDocument::ImageSize() const {
  DCHECK(image_element_);
  DCHECK(image_element_->CachedImage());
  return image_element_->CachedImage()->IntrinsicSize(
      LayoutObject::ShouldRespectImageOrientation(
          image_element_->GetLayoutObject()));
}

void ImageDocument::CreateDocumentStructure() {
  auto* root_element = MakeGarbageCollected<HTMLHtmlElement>(*this);
  AppendChild(root_element);
  root_element->InsertedByParser();

  if (IsStopped())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  auto* head = MakeGarbageCollected<HTMLHeadElement>(*this);
  auto* meta = MakeGarbageCollected<HTMLMetaElement>(*this);
  meta->setAttribute(html_names::kNameAttr, "viewport");
  meta->setAttribute(html_names::kContentAttr,
                     "width=device-width, minimum-scale=0.1");
  head->AppendChild(meta);

  auto* body = MakeGarbageCollected<HTMLBodyElement>(*this);

  if (ShouldShrinkToFit()) {
    // Display the image prominently centered in the frame.
    body->setAttribute(html_names::kStyleAttr,
                       "margin: 0px; background: #0e0e0e;");

    // See w3c example on how to center an element:
    // https://www.w3.org/Style/Examples/007/center.en.html
    div_element_ = MakeGarbageCollected<HTMLDivElement>(*this);
    div_element_->setAttribute(html_names::kStyleAttr,
                               "display: flex;"
                               "flex-direction: column;"
                               "align-items: flex-start;"
                               "min-width: min-content;"
                               "height: 100%;"
                               "width: 100%;");
    HTMLSlotElement* slot = HTMLSlotElement::CreateUserAgentDefaultSlot(*this);
    div_element_->AppendChild(slot);

    // Adding a UA shadow root here is because the container <div> should be
    // hidden so that only the <img> element should be visible in <body>,
    // according to the spec:
    // https://html.spec.whatwg.org/C/#read-media
    ShadowRoot& shadow_root = body->EnsureUserAgentShadowRoot();
    shadow_root.AppendChild(div_element_);
  } else {
    body->setAttribute(html_names::kStyleAttr, "margin: 0px;");
  }

  WillInsertBody();

  image_element_ = MakeGarbageCollected<HTMLImageElement>(*this);
  UpdateImageStyle();
  image_element_->SetLoadingImageDocument();
  image_element_->setAttribute(html_names::kSrcAttr,
                               AtomicString(Url().GetString()));
  body->AppendChild(image_element_.Get());
  if (Loader() && image_element_->CachedImageResourceForImageDocument()) {
    image_element_->CachedImageResourceForImageDocument()->ResponseReceived(
        Loader()->GetResponse());
  }

  if (ShouldShrinkToFit()) {
    // Add event listeners
    auto* listener = MakeGarbageCollected<ImageEventListener>(this);
    if (LocalDOMWindow* dom_window = domWindow())
      dom_window->addEventListener(event_type_names::kResize, listener, false);

    if (shrink_to_fit_mode_ == kDesktop) {
      image_element_->addEventListener(event_type_names::kClick, listener,
                                       false);
    } else if (shrink_to_fit_mode_ == kViewport) {
      image_element_->addEventListener(event_type_names::kTouchend, listener,
                                       false);
      image_element_->addEventListener(event_type_names::kTouchcancel, listener,
                                       false);
    }
  }

  root_element->AppendChild(head);
  root_element->AppendChild(body);
}

float ImageDocument::Scale() const {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  if (!image_element_ || image_element_->GetDocument() != this)
    return 1.0f;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 1.0f;

  IntSize image_size = ImageSize();
  if (image_size.IsEmpty())
    return 1.0f;

  // We want to pretend the viewport is larger when the user has zoomed the
  // page in (but not when the zoom is coming from device scale).
  const float viewport_zoom =
      view->GetChromeClient()->WindowToViewportScalar(GetFrame(), 1.f);
  float width_scale = view->Width() / (viewport_zoom * image_size.Width());
  float height_scale = view->Height() / (viewport_zoom * image_size.Height());

  return std::min(width_scale, height_scale);
}

void ImageDocument::ResizeImageToFit() {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  if (!image_element_ || image_element_->GetDocument() != this)
    return;

  IntSize image_size = ImageSize();
  image_size.Scale(Scale());

  image_element_->setWidth(image_size.Width());
  image_element_->setHeight(image_size.Height());

  UpdateImageStyle();
}

void ImageDocument::ImageClicked(int x, int y) {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);

  if (!image_size_is_known_ || ImageFitsInWindow())
    return;

  should_shrink_image_ = !should_shrink_image_;

  if (should_shrink_image_) {
    WindowSizeChanged();
  } else {
    // Adjust the coordinates to account for the fact that the image was
    // centered on the screen.
    float image_x = x - image_element_->OffsetLeft();
    float image_y = y - image_element_->OffsetTop();

    RestoreImageSize();

    UpdateStyleAndLayout();

    double scale = Scale();
    double device_scale_factor =
        GetFrame()->View()->GetChromeClient()->WindowToViewportScalar(
            GetFrame(), 1.f);

    float scroll_x = (image_x * device_scale_factor) / scale -
                     static_cast<float>(GetFrame()->View()->Width()) / 2;
    float scroll_y = (image_y * device_scale_factor) / scale -
                     static_cast<float>(GetFrame()->View()->Height()) / 2;

    GetFrame()->View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(scroll_x, scroll_y), kProgrammaticScroll);
  }
}

void ImageDocument::ImageLoaded() {
  image_is_loaded_ = true;

  if (ShouldShrinkToFit()) {
    // The checkerboard background needs to be inserted.
    UpdateImageStyle();
  }
}

void ImageDocument::UpdateImageStyle() {
  StringBuilder image_style;
  image_style.Append("-webkit-user-select: none;");

  if (ShouldShrinkToFit()) {
    if (shrink_to_fit_mode_ == kViewport)
      image_style.Append("max-width: 100%;");
    image_style.Append("margin: auto;");

    if (image_is_loaded_) {
      MouseCursorMode new_cursor_mode = kDefault;

      if (shrink_to_fit_mode_ != kViewport) {
        // In desktop mode, the user can click on the image to zoom in or out.
        DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
        if (ImageFitsInWindow()) {
          new_cursor_mode = kDefault;
        } else {
          new_cursor_mode = should_shrink_image_ ? kZoomIn : kZoomOut;
        }
      }

      // The only thing that can differ between updates is
      // the type of cursor being displayed.
      if (new_cursor_mode == style_mouse_cursor_mode_) {
        return;
      }
      style_mouse_cursor_mode_ = new_cursor_mode;

      if (shrink_to_fit_mode_ == kDesktop) {
        if (style_mouse_cursor_mode_ == kZoomIn)
          image_style.Append("cursor: zoom-in;");
        else if (style_mouse_cursor_mode_ == kZoomOut)
          image_style.Append("cursor: zoom-out;");
      }
    }
  }

  image_element_->setAttribute(html_names::kStyleAttr,
                               image_style.ToAtomicString());
}

void ImageDocument::ImageUpdated() {
  DCHECK(image_element_);

  if (image_size_is_known_)
    return;

  UpdateStyleAndLayoutTree();
  if (!image_element_->CachedImage() || ImageSize().IsEmpty())
    return;

  image_size_is_known_ = true;

  if (ShouldShrinkToFit()) {
    // Force resizing of the image
    WindowSizeChanged();
  }
}

void ImageDocument::RestoreImageSize() {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);

  if (!image_element_ || !image_size_is_known_ ||
      image_element_->GetDocument() != this)
    return;

  IntSize image_size = ImageSize();
  image_element_->setWidth(image_size.Width());
  image_element_->setHeight(image_size.Height());
  UpdateImageStyle();

  did_shrink_image_ = false;
}

bool ImageDocument::ImageFitsInWindow() const {
  DCHECK_EQ(shrink_to_fit_mode_, kDesktop);
  return Scale() >= 1;
}

int ImageDocument::CalculateDivWidth() {
  // Zooming in and out of an image being displayed within a viewport is done
  // by changing the page scale factor of the page instead of changing the
  // size of the image.  The size of the image is set so that:
  // * Images wider than the viewport take the full width of the screen.
  // * Images taller than the viewport are initially aligned with the top of
  //   of the frame.
  // * Images smaller in either dimension are centered along that axis.
  int viewport_width =
      GetFrame()->GetPage()->GetVisualViewport().Size().Width() /
      GetFrame()->PageZoomFactor();

  // For huge images, minimum-scale=0.1 is still too big on small screens.
  // Set the <div> width so that the image will shrink to fit the width of the
  // screen when the scale is minimum.
  int max_width = std::min(ImageSize().Width(), viewport_width * 10);
  return std::max(viewport_width, max_width);
}

void ImageDocument::WindowSizeChanged() {
  if (!image_element_ || !image_size_is_known_ ||
      image_element_->GetDocument() != this)
    return;

  if (shrink_to_fit_mode_ == kViewport) {
    int div_width = CalculateDivWidth();
    div_element_->SetInlineStyleProperty(CSSPropertyID::kWidth, div_width,
                                         CSSPrimitiveValue::UnitType::kPixels);

    // Explicitly set the height of the <div> containing the <img> so that it
    // can display the full image without shrinking it, allowing a full-width
    // reading mode for normal-width-huge-height images. Use the LayoutSize
    // for height rather than viewport since that doesn't change based on the
    // URL bar coming in and out - thus preventing the image from jumping
    // around. i.e. The div should fill the viewport when minimally zoomed and
    // the URL bar is showing, but won't fill the new space when the URL bar
    // hides.
    float aspect_ratio = View()->GetLayoutSize().AspectRatio();
    int div_height = std::max(ImageSize().Height(),
                              static_cast<int>(div_width / aspect_ratio));
    div_element_->SetInlineStyleProperty(CSSPropertyID::kHeight, div_height,
                                         CSSPrimitiveValue::UnitType::kPixels);
    return;
  }

  bool fits_in_window = ImageFitsInWindow();

  // If the image has been explicitly zoomed in, restore the cursor if the image
  // fits and set it to a zoom out cursor if the image doesn't fit
  if (!should_shrink_image_) {
    UpdateImageStyle();
    return;
  }

  if (did_shrink_image_) {
    // If the window has been resized so that the image fits, restore the image
    // size otherwise update the restored image size.
    if (fits_in_window)
      RestoreImageSize();
    else
      ResizeImageToFit();
  } else {
    // If the image isn't resized but needs to be, then resize it.
    if (!fits_in_window) {
      ResizeImageToFit();
      did_shrink_image_ = true;
    }
  }
}

ImageResourceContent* ImageDocument::CachedImage() {
  if (!image_element_) {
    CreateDocumentStructure();
    if (IsStopped()) {
      image_element_ = nullptr;
      return nullptr;
    }
  }

  return image_element_->CachedImage();
}

ImageResource* ImageDocument::CachedImageResourceDeprecated() {
  if (!image_element_) {
    CreateDocumentStructure();
    if (IsStopped()) {
      image_element_ = nullptr;
      return nullptr;
    }
  }

  return image_element_->CachedImageResourceForImageDocument();
}

bool ImageDocument::ShouldShrinkToFit() const {
  // WebView automatically resizes to match the contents, causing an infinite
  // loop as the contents then resize to match the window. To prevent this,
  // disallow images from shrinking to fit for WebViews.
  bool is_wrap_content_web_view =
      GetPage() ? GetPage()->GetSettings().GetForceZeroLayoutHeight() : false;
  return GetFrame()->IsMainFrame() && !is_wrap_content_web_view;
}

void ImageDocument::Trace(Visitor* visitor) {
  visitor->Trace(div_element_);
  visitor->Trace(image_element_);
  HTMLDocument::Trace(visitor);
}

// --------

void ImageEventListener::Invoke(ExecutionContext*, Event* event) {
  if (event->type() == event_type_names::kResize) {
    doc_->WindowSizeChanged();
  } else if (event->type() == event_type_names::kClick &&
             event->IsMouseEvent()) {
    MouseEvent* mouse_event = ToMouseEvent(event);
    doc_->ImageClicked(mouse_event->x(), mouse_event->y());
  } else if ((event->type() == event_type_names::kTouchend ||
              event->type() == event_type_names::kTouchcancel) &&
             event->IsTouchEvent()) {
    doc_->UpdateImageStyle();
  }
}

bool ImageEventListener::Matches(const EventListener& listener) const {
  if (const ImageEventListener* image_event_listener =
          DynamicTo<ImageEventListener>(listener)) {
    return doc_ == image_event_listener->doc_;
  }
  return false;
}

}  // namespace blink
