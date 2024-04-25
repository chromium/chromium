// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_fallback_helper.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

static bool ImageRepresentsNothing(const StyleUAShadowHostData& host_data) {
  // We source fallback content/alternative text from more than just the 'alt'
  // attribute, so consider the element to represent text in those cases as
  // well.
  bool alt_is_set = !host_data.AltText().IsNull();
  bool alt_is_empty = alt_is_set && host_data.AltText().empty();
  bool src_is_set = !host_data.SrcAttribute().empty();
  if (src_is_set && alt_is_empty) {
    return true;
  }
  return !src_is_set && (!alt_is_set || alt_is_empty);
}

static bool ImageSmallerThanAltImage(int pixels_for_alt_image,
                                     const Length& width,
                                     const Length& height) {
  // We don't have a layout tree so can't compute the size of an image
  // relative dimensions - so we just assume we should display the alt image.
  if (!width.IsFixed() && !height.IsFixed())
    return false;
  if (height.IsFixed() && height.Value() < pixels_for_alt_image)
    return true;
  return width.IsFixed() && width.Value() < pixels_for_alt_image;
}

static bool TreatImageAsReplaced(const Document& document,
                                 const StyleUAShadowHostData& host_data) {
  // TODO(https://crbug.com/313072): Is this still correct in the presence of
  // intrinsic sizing keywords or calc-size?
  bool has_intrinsic_dimensions =
      !host_data.Width().IsAuto() && !host_data.Height().IsAuto();
  bool has_dimensions_from_ar =
      !host_data.AspectRatio().IsAuto() &&
      (!host_data.Width().IsAuto() || !host_data.Height().IsAuto());
  bool has_no_alt_attribute = host_data.AltAttribute().empty();
  return (has_intrinsic_dimensions || has_dimensions_from_ar) &&
         (document.InQuirksMode() || has_no_alt_attribute);
}

namespace {

void AdjustChildStyle(Element&, ComputedStyleBuilder&);

class HTMLAltTextContainerElement : public HTMLSpanElement {
 public:
  explicit HTMLAltTextContainerElement(Document& document)
      : HTMLSpanElement(document) {
    SetHasCustomStyleCallbacks();
  }

  void AdjustStyle(ComputedStyleBuilder& builder) override {
    AdjustChildStyle(*this, builder);
  }
};

class HTMLAltTextImageElement : public HTMLImageElement {
 public:
  explicit HTMLAltTextImageElement(Document& document)
      : HTMLImageElement(document) {
    SetHasCustomStyleCallbacks();
  }

  void AdjustStyle(ComputedStyleBuilder& builder) override {
    AdjustChildStyle(*this, builder);
  }
};

class ImageFallbackContentBuilder {
  STACK_ALLOCATED();

 public:
  explicit ImageFallbackContentBuilder(ComputedStyleBuilder& builder)
      : builder_(builder) {}

  void ShowBrokenImageIcon(bool is_ltr) {
    // alttext-image

    // Note that floating elements are blockified by StyleAdjuster.
    builder_.SetDisplay(EDisplay::kBlock);

    // Make sure the broken image icon appears on the appropriate side of the
    // image for the element's writing direction.
    builder_.SetFloating(is_ltr ? EFloat::kLeft : EFloat::kRight);
  }
  void HideBrokenImageIcon() {
    // alttext-image

    builder_.SetDisplay(EDisplay::kNone);
  }

  void ShowAsReplaced(const Length& width, const Length& height) {
    // alttext-container

    builder_.SetOverflowX(EOverflow::kHidden);
    builder_.SetOverflowY(EOverflow::kHidden);
    builder_.SetDisplay(EDisplay::kInlineBlock);
    builder_.SetPointerEvents(EPointerEvents::kNone);
    builder_.SetHeight(height);
    builder_.SetWidth(width);
    // Text decorations must be reset for for inline-block,
    // see StopPropagateTextDecorations in style_adjuster.cc.
    builder_.SetBaseTextDecorationData(nullptr);
  }

  void ShowBorder(float zoom) {
    // alttext-container

    int border_width = static_cast<int>(1 * zoom);
    builder_.SetBorderTopWidth(border_width);
    builder_.SetBorderRightWidth(border_width);
    builder_.SetBorderBottomWidth(border_width);
    builder_.SetBorderLeftWidth(border_width);

    EBorderStyle border_style = EBorderStyle::kSolid;
    builder_.SetBorderTopStyle(border_style);
    builder_.SetBorderRightStyle(border_style);
    builder_.SetBorderBottomStyle(border_style);
    builder_.SetBorderLeftStyle(border_style);

    StyleColor border_color(CSSValueID::kSilver);
    builder_.SetBorderTopColor(border_color);
    builder_.SetBorderRightColor(border_color);
    builder_.SetBorderBottomColor(border_color);
    builder_.SetBorderLeftColor(border_color);

    Length padding = Length::Fixed(1 * zoom);
    builder_.SetPaddingTop(padding);
    builder_.SetPaddingRight(padding);
    builder_.SetPaddingBottom(padding);
    builder_.SetPaddingLeft(padding);

    builder_.SetBoxSizing(EBoxSizing::kBorderBox);
  }

  void AlignToBaseline() {
    // alttext-container

    builder_.SetVerticalAlign(EVerticalAlign::kBaseline);
  }

 private:
  ComputedStyleBuilder& builder_;
};

}  // namespace

void HTMLImageFallbackHelper::CreateAltTextShadowTree(Element& element) {
  Document& document = element.GetDocument();

  auto* container = MakeGarbageCollected<HTMLAltTextContainerElement>(document);
  container->setAttribute(html_names::kIdAttr,
                          AtomicString("alttext-container"));

  auto* broken_image = MakeGarbageCollected<HTMLAltTextImageElement>(document);
  broken_image->SetIsFallbackImage();
  broken_image->setAttribute(html_names::kIdAttr,
                             AtomicString("alttext-image"));
  broken_image->setAttribute(html_names::kWidthAttr, AtomicString("16"));
  broken_image->setAttribute(html_names::kHeightAttr, AtomicString("16"));
  broken_image->setAttribute(html_names::kAlignAttr, AtomicString("left"));
  broken_image->SetInlineStyleProperty(CSSPropertyID::kMargin, 0,
                                       CSSPrimitiveValue::UnitType::kPixels);
  container->AppendChild(broken_image);

  auto* alt_text = MakeGarbageCollected<HTMLSpanElement>(document);
  alt_text->setAttribute(html_names::kIdAttr, AtomicString("alttext"));

  auto* text = Text::Create(document, To<HTMLElement>(element).AltText());
  alt_text->AppendChild(text);
  container->AppendChild(alt_text);

  element.EnsureUserAgentShadowRoot().AppendChild(container);
}

void HTMLImageFallbackHelper::AdjustHostStyle(HTMLElement& element,
                                              ComputedStyleBuilder& builder) {
  // If we have an author shadow root or have not created the UA shadow root
  // yet, bail early. We can't use EnsureUserAgentShadowRoot() here because that
  // would alter the DOM tree during style recalc.
  if (element.AuthorShadowRoot() || !element.UserAgentShadowRoot()) {
    builder.SetUAShadowHostData(nullptr);
    return;
  }

  if (element.GetDocument().InQuirksMode()) {
    // Mimic the behaviour of the image host by setting symmetric dimensions if
    // only one dimension is specified.
    // TODO(https://crbug.com/313072): Is this still correct in the presence
    // of intrinsic sizing keywords or calc-size?
    if (!builder.Width().IsAuto() && builder.Height().IsAuto()) {
      builder.SetHeight(builder.Width());
    } else if (!builder.Height().IsAuto() && builder.Width().IsAuto()) {
      builder.SetWidth(builder.Height());
    }
  }

  // This data will be inherited to all descendants of `element`, and will
  // be available during subsequent calls to `AdjustChildStyle`.
  builder.SetUAShadowHostData(std::make_unique<StyleUAShadowHostData>(
      builder.Width(), builder.Height(), builder.AspectRatio(),
      element.AltText(), element.getAttribute(html_names::kAltAttr),
      element.getAttribute(html_names::kSrcAttr)));

  if (!TreatImageAsReplaced(element.GetDocument(),
                            *builder.UAShadowHostData())) {
    if (builder.Display() == EDisplay::kInline) {
      builder.SetWidth(Length());
      builder.SetHeight(Length());
      builder.SetAspectRatio(ComputedStyleInitialValues::InitialAspectRatio());
    }
  }
}

namespace {

void AdjustChildStyle(Element& element, ComputedStyleBuilder& builder) {
  const AtomicString& id = element.GetIdAttribute();
  bool is_alttext_container = id == "alttext-container";
  bool is_alttext_image = id == "alttext-image";
  DCHECK_NE(is_alttext_container, is_alttext_image);

  if (!builder.UAShadowHostData()) {
    return;
  }

  const StyleUAShadowHostData& host_data = *builder.UAShadowHostData();

  ShadowRoot* root = element.ContainingShadowRoot();
  DCHECK(root && root->IsUserAgent());

  ImageFallbackContentBuilder fallback(builder);

  if (is_alttext_container) {
    if (element.GetDocument().InQuirksMode() && !host_data.Width().IsAuto() &&
        !host_data.Height().IsAuto()) {
      fallback.AlignToBaseline();
    }
  }

  if (TreatImageAsReplaced(element.GetDocument(), host_data)) {
    // https://html.spec.whatwg.org/C/#images-3:
    // "If the element does not represent an image, but the element already has
    // intrinsic dimensions (e.g. from the dimension attributes or CSS rules),
    // and either: the user agent has reason to believe that the image will
    // become available and be rendered in due course, or the element has no alt
    // attribute, or the Document is in quirks mode The user agent is expected
    // to treat the element as a replaced element whose content is the text that
    // the element represents, if any."
    if (is_alttext_container) {
      fallback.ShowAsReplaced(host_data.Width(), host_data.Height());
    }

    // 16px for the image and 2px for its top/left border/padding offset.
    int pixels_for_alt_image = 18;
    if (ImageSmallerThanAltImage(pixels_for_alt_image, host_data.Width(),
                                 host_data.Height())) {
      if (is_alttext_image) {
        fallback.HideBrokenImageIcon();
      }
    } else {
      if (is_alttext_container) {
        fallback.ShowBorder(builder.EffectiveZoom());
      } else {
        DCHECK(is_alttext_image);
        fallback.ShowBrokenImageIcon(builder.Direction() ==
                                     TextDirection::kLtr);
      }
    }
  } else {
    if (ImageRepresentsNothing(host_data)) {
      // "If the element is an img element that represents nothing and the user
      // agent does not expect this to change the user agent is expected to
      // treat the element as an empty inline element."
      //  - We achieve this by hiding the broken image so that the span is
      //  empty.
      if (is_alttext_image) {
        fallback.HideBrokenImageIcon();
      }
    } else {
      // "If the element is an img element that represents some text and the
      // user agent does not expect this to change the user agent is expected to
      // treat the element as a non-replaced phrasing element whose content is
      // the text, optionally with an icon indicating that an image is missing,
      // so that the user can request the image be displayed or investigate why
      // it is not rendering."
      if (is_alttext_image) {
        fallback.ShowBrokenImageIcon(builder.Direction() ==
                                     TextDirection::kLtr);
      }
    }
  }
}

}  // namespace

}  // namespace blink
