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

static bool ImageSmallerThanAltImage(const Length& width,
                                     const Length& height) {
  // 16px for the image and 2px for its top/left border/padding offset.
  const int kPixelsForAltImage = 18;

  // We don't have a layout tree so can't compute the size of an image
  // relative dimensions - so we just assume we should display the alt image.
  if (!width.IsFixed() && !height.IsFixed()) {
    return false;
  }
  if (height.IsFixed() && height.Value() < kPixelsForAltImage) {
    return true;
  }
  return width.IsFixed() && width.Value() < kPixelsForAltImage;
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

class HTMLAltTextContainerElement : public HTMLSpanElement {
 public:
  explicit HTMLAltTextContainerElement(Document& document)
      : HTMLSpanElement(document) {
    SetHasCustomStyleCallbacks();
  }

  void AdjustStyle(ComputedStyleBuilder& builder) override {
    if (!builder.UAShadowHostData()) {
      return;
    }

    const StyleUAShadowHostData& host_data = *builder.UAShadowHostData();

    if (GetDocument().InQuirksMode() && !host_data.Width().IsAuto() &&
        !host_data.Height().IsAuto()) {
      AlignToBaseline(builder);
    }

    if (TreatImageAsReplaced(GetDocument(), host_data)) {
      // https://html.spec.whatwg.org/C/#images-3:
      // "If the element does not represent an image, but the element already
      // has intrinsic dimensions (e.g. from the dimension attributes or CSS
      // rules), and either: the user agent has reason to believe that the image
      // will become available and be rendered in due course, or the element has
      // no alt attribute, or the Document is in quirks mode The user agent is
      // expected to treat the element as a replaced element whose content is
      // the text that the element represents, if any."
      ShowAsReplaced(builder, host_data.Width(), host_data.Height());

      if (!ImageSmallerThanAltImage(host_data.Width(), host_data.Height())) {
        ShowBorder(builder);
      }
    }
  }

 private:
  void ShowAsReplaced(ComputedStyleBuilder& builder,
                      const Length& width,
                      const Length& height) {
    builder.SetOverflowX(EOverflow::kHidden);
    builder.SetOverflowY(EOverflow::kHidden);
    builder.SetDisplay(EDisplay::kInlineBlock);
    builder.SetPointerEvents(EPointerEvents::kNone);
    builder.SetHeight(height);
    builder.SetWidth(width);
    // Text decorations must be reset for for inline-block,
    // see StopPropagateTextDecorations in style_adjuster.cc.
    builder.SetBaseTextDecorationData(nullptr);
  }

  void ShowBorder(ComputedStyleBuilder& builder) {
    int border_width = static_cast<int>(builder.EffectiveZoom());
    builder.SetBorderTopWidth(border_width);
    builder.SetBorderRightWidth(border_width);
    builder.SetBorderBottomWidth(border_width);
    builder.SetBorderLeftWidth(border_width);

    EBorderStyle border_style = EBorderStyle::kSolid;
    builder.SetBorderTopStyle(border_style);
    builder.SetBorderRightStyle(border_style);
    builder.SetBorderBottomStyle(border_style);
    builder.SetBorderLeftStyle(border_style);

    StyleColor border_color(CSSValueID::kSilver);
    builder.SetBorderTopColor(border_color);
    builder.SetBorderRightColor(border_color);
    builder.SetBorderBottomColor(border_color);
    builder.SetBorderLeftColor(border_color);

    Length padding = Length::Fixed(builder.EffectiveZoom());
    builder.SetPaddingTop(padding);
    builder.SetPaddingRight(padding);
    builder.SetPaddingBottom(padding);
    builder.SetPaddingLeft(padding);

    builder.SetBoxSizing(EBoxSizing::kBorderBox);
  }

  void AlignToBaseline(ComputedStyleBuilder& builder) {
    builder.SetVerticalAlign(EVerticalAlign::kBaseline);
  }
};

class HTMLAltTextImageElement : public HTMLImageElement {
 public:
  explicit HTMLAltTextImageElement(Document& document)
      : HTMLImageElement(document) {
    SetHasCustomStyleCallbacks();
  }

  void AdjustStyle(ComputedStyleBuilder& builder) override {
    if (!builder.UAShadowHostData()) {
      return;
    }

    const StyleUAShadowHostData& host_data = *builder.UAShadowHostData();

    if (TreatImageAsReplaced(GetDocument(), host_data)) {
      if (ImageSmallerThanAltImage(host_data.Width(), host_data.Height())) {
        HideBrokenImageIcon(builder);
      } else {
        ShowBrokenImageIcon(builder);
      }
    } else {
      if (ImageRepresentsNothing(host_data)) {
        // "If the element is an img element that represents nothing and the
        // user agent does not expect this to change the user agent is expected
        // to treat the element as an empty inline element."
        //  - We achieve this by hiding the broken image so that the span is
        //  empty.
        HideBrokenImageIcon(builder);
      } else {
        // "If the element is an img element that represents some text and the
        // user agent does not expect this to change the user agent is expected
        // to treat the element as a non-replaced phrasing element whose content
        // is the text, optionally with an icon indicating that an image is
        // missing, so that the user can request the image be displayed or
        // investigate why it is not rendering."
        ShowBrokenImageIcon(builder);
      }
    }
  }

 private:
  void ShowBrokenImageIcon(ComputedStyleBuilder& builder) {
    // See AdjustStyleForDisplay() in style_adjuster.cc.
    if (builder.IsInInlinifyingDisplay()) {
      builder.SetDisplay(EDisplay::kInline);
      builder.SetFloating(EFloat::kNone);
      return;
    }

    // Note that floating elements are blockified by StyleAdjuster.
    builder.SetDisplay(EDisplay::kBlock);

    // Make sure the broken image icon appears on the appropriate side of the
    // image for the element's writing direction.
    bool is_ltr = builder.Direction() == TextDirection::kLtr;
    builder.SetFloating(is_ltr ? EFloat::kLeft : EFloat::kRight);
  }

  void HideBrokenImageIcon(ComputedStyleBuilder& builder) {
    builder.SetDisplay(EDisplay::kNone);
  }
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
      element.getAttribute(html_names::kSrcAttr), /* has_appearance */ false));

  if (!TreatImageAsReplaced(element.GetDocument(),
                            *builder.UAShadowHostData())) {
    if (builder.Display() == EDisplay::kInline) {
      builder.SetWidth(Length());
      builder.SetHeight(Length());
      builder.SetAspectRatio(ComputedStyleInitialValues::InitialAspectRatio());
    }
  }
}

}  // namespace blink
