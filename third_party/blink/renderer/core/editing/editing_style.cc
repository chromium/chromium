/*
 * Copyright (C) 2007, 2008, 2009 Apple Computer, Inc.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editing_style.h"

#include "base/memory/values_equivalent.h"
#include "base/stl_util.h"
#include "mojo/public/mojom/base/text_direction.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/editing_style_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_font_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be
// copied to the new paragraph.
// NOTE: Use either allEditingProperties() or inheritableEditingProperties() to
// respect runtime enabling of properties.
static const CSSPropertyID kStaticEditingProperties[] = {
    CSSPropertyID::kBackgroundColor,
    CSSPropertyID::kColor,
    CSSPropertyID::kFontFamily,
    CSSPropertyID::kFontSize,
    CSSPropertyID::kFontStyle,
    CSSPropertyID::kFontVariantLigatures,
    CSSPropertyID::kFontVariantCaps,
    CSSPropertyID::kFontWeight,
    CSSPropertyID::kLetterSpacing,
    CSSPropertyID::kOrphans,
    CSSPropertyID::kTextAlign,
    CSSPropertyID::kTextDecorationLine,
    CSSPropertyID::kTextIndent,
    CSSPropertyID::kTextTransform,
    CSSPropertyID::kWidows,
    CSSPropertyID::kWordSpacing,
    CSSPropertyID::kWebkitTextDecorationsInEffect,
    CSSPropertyID::kWebkitTextFillColor,
    CSSPropertyID::kWebkitTextStrokeColor,
    CSSPropertyID::kWebkitTextStrokeWidth,
    CSSPropertyID::kCaretColor,
    CSSPropertyID::kTextWrapMode,
    CSSPropertyID::kWhiteSpaceCollapse,
};

enum EditingPropertiesType {
  kOnlyInheritableEditingProperties,
  kAllEditingProperties
};

static const Vector<const CSSProperty*>& AllEditingProperties(
    const ExecutionContext* execution_context) {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  if (properties.empty()) {
    properties.ReserveInitialCapacity(std::size(kStaticEditingProperties) + 2);
    CSSProperty::FilterWebExposedCSSPropertiesIntoVector(
        execution_context, kStaticEditingProperties,
        std::size(kStaticEditingProperties), properties);
  }
  return properties;
}

static const Vector<const CSSProperty*>& InheritableEditingProperties(
    const ExecutionContext* execution_context) {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  if (properties.empty()) {
    const Vector<const CSSProperty*>& all =
        AllEditingProperties(execution_context);
    properties.ReserveInitialCapacity(all.size());
    for (const CSSProperty* property : all) {
      if (property->IsInherited()) {
        properties.push_back(property);
      }
    }
  }
  return properties;
}

template <class StyleDeclarationType>
static MutableCSSPropertyValueSet* CopyEditingProperties(
    const ExecutionContext* execution_context,
    StyleDeclarationType* style,
    EditingPropertiesType type = kOnlyInheritableEditingProperties) {
  if (type == kAllEditingProperties)
    return style->CopyPropertiesInSet(AllEditingProperties(execution_context));
  return style->CopyPropertiesInSet(
      InheritableEditingProperties(execution_context));
}

static inline bool IsEditingProperty(ExecutionContext* execution_context,
                                     CSSPropertyID id) {
  static const Vector<const CSSProperty*>& properties =
      AllEditingProperties(execution_context);
  for (wtf_size_t index = 0; index < properties.size(); index++) {
    if (properties[index]->IDEquals(id))
      return true;
  }
  return false;
}

static MutableCSSPropertyValueSet* GetPropertiesNotIn(
    CSSPropertyValueSet* style_with_redundant_properties,
    Node*,
    CSSStyleDeclaration* base_style,
    SecureContextMode);
enum LegacyFontSizeMode {
  kAlwaysUseLegacyFontSize,
  kUseLegacyFontSizeOnlyIfPixelValuesMatch
};
static int LegacyFontSizeFromCSSValue(Document*,
                                      const CSSValue*,
                                      bool,
                                      LegacyFontSizeMode);

class HTMLElementEquivalent : public GarbageCollected<HTMLElementEquivalent> {
 public:
  HTMLElementEquivalent(CSSPropertyID);
  HTMLElementEquivalent(CSSPropertyID, const HTMLQualifiedName& tag_name);
  HTMLElementEquivalent(CSSPropertyID,
                        CSSValueID primitive_value,
                        const HTMLQualifiedName& tag_name);

  virtual bool Matches(const Element* element) const {
    return !tag_name_ || element->HasTagName(*tag_name_);
  }
  virtual bool HasAttribute() const { return false; }
  virtual bool PropertyExistsInStyle(const CSSPropertyValueSet* style) const {
    return style->GetPropertyCSSValue(property_id_);
  }
  virtual bool ValueIsPresentInStyle(HTMLElement*, CSSPropertyValueSet*) const;
  virtual void AddToStyle(Element*, EditingStyle*) const;

  virtual void Trace(Visitor* visitor) const {
    visitor->Trace(identifier_value_);
  }

 protected:
  const CSSPropertyID property_id_;
  const Member<CSSIdentifierValue> identifier_value_;
  // We can store a pointer because HTML tag names are const global.
  const HTMLQualifiedName* tag_name_;
};

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id)
    : property_id_(id), tag_name_(nullptr) {}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id,
                                             const HTMLQualifiedName& tag_name)
    : property_id_(id), tag_name_(&tag_name) {}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id,
                                             CSSValueID value_id,
                                             const HTMLQualifiedName& tag_name)
    : property_id_(id),
      identifier_value_(CSSIdentifierValue::Create(value_id)),
      tag_name_(&tag_name) {
  DCHECK(IsValidCSSValueID(value_id));
}

bool HTMLElementEquivalent::ValueIsPresentInStyle(
    HTMLElement* element,
    CSSPropertyValueSet* style) const {
  const CSSValue* value = style->GetPropertyCSSValue(property_id_);

  // TODO: Does this work on style or computed style? The code here, but we
  // might need to do something here to match CSSPrimitiveValues. if
  // (property_id_ == CSSPropertyID::kFontWeight &&
  //     identifier_value_->GetValueID() == CSSValueID::kBold) {
  //   auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  //   if (primitive_value &&
  //       primitive_value->GetFloatValue() >= BoldThreshold()) {
  //     LOG(INFO) << "weight match in HTMLElementEquivalent for primitive
  //     value"; return true;
  //   } else {
  //     LOG(INFO) << "weight match in HTMLElementEquivalent for identifier
  //     value";
  //   }
  // }

  if (!Matches(element))
    return false;

  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value &&
         identifier_value->GetValueID() == identifier_value_->GetValueID();
}

void HTMLElementEquivalent::AddToStyle(Element* element,
                                       EditingStyle* style) const {
  style->SetProperty(property_id_, identifier_value_->CssText(),
                     /* important */ false,
                     element->GetExecutionContext()->GetSecureContextMode());
}

class HTMLTextDecorationEquivalent final : public HTMLElementEquivalent {
 public:
  static HTMLElementEquivalent* Create(CSSValueID primitive_value,
                                       const HTMLQualifiedName& tag_name) {
    return MakeGarbageCollected<HTMLTextDecorationEquivalent>(primitive_value,
                                                              tag_name);
  }

  HTMLTextDecorationEquivalent(CSSValueID primitive_value,
                               const HTMLQualifiedName& tag_name);

  bool PropertyExistsInStyle(const CSSPropertyValueSet*) const override;
  bool ValueIsPresentInStyle(HTMLElement*, CSSPropertyValueSet*) const override;

  void Trace(Visitor* visitor) const override {
    HTMLElementEquivalent::Trace(visitor);
  }
};

HTMLTextDecorationEquivalent::HTMLTextDecorationEquivalent(
    CSSValueID primitive_value,
    const HTMLQualifiedName& tag_name)
    : HTMLElementEquivalent(CSSPropertyID::kTextDecorationLine,
                            primitive_value,
                            tag_name)
// CSSPropertyID::kTextDecorationLine is used in
// HTMLElementEquivalent::AddToStyle
{}

bool HTMLTextDecorationEquivalent::PropertyExistsInStyle(
    const CSSPropertyValueSet* style) const {
  return style->GetPropertyCSSValue(
             CSSPropertyID::kWebkitTextDecorationsInEffect) ||
         style->GetPropertyCSSValue(CSSPropertyID::kTextDecorationLine);
}

bool HTMLTextDecorationEquivalent::ValueIsPresentInStyle(
    HTMLElement* element,
    CSSPropertyValueSet* style) const {
  const CSSValue* style_value =
      style->GetPropertyCSSValue(CSSPropertyID::kWebkitTextDecorationsInEffect);
  if (!style_value) {
    style_value =
        style->GetPropertyCSSValue(CSSPropertyID::kTextDecorationLine);
  }
  if (!Matches(element))
    return false;
  auto* style_value_list = DynamicTo<CSSValueList>(style_value);
  return style_value_list && style_value_list->HasValue(*identifier_value_);
}

class HTMLAttributeEquivalent : public HTMLElementEquivalent {
 public:
  HTMLAttributeEquivalent(CSSPropertyID,
                          const HTMLQualifiedName& tag_name,
                          const QualifiedName& attr_name);
  HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& attr_name);

  bool Matches(const Element* element) const override {
    return HTMLElementEquivalent::Matches(element) &&
           element->hasAttribute(attr_name_);
  }
  bool HasAttribute() const override { return true; }
  bool ValueIsPresentInStyle(HTMLElement*, CSSPropertyValueSet*) const override;
  void AddToStyle(Element*, EditingStyle*) const override;
  virtual const CSSValue* AttributeValueAsCSSValue(Element*) const;
  inline const QualifiedName& AttributeName() const { return attr_name_; }

  void Trace(Visitor* visitor) const override {
    HTMLElementEquivalent::Trace(visitor);
  }

 protected:
  // We can store a reference because HTML attribute names are const global.
  const QualifiedName& attr_name_;
};

HTMLAttributeEquivalent::HTMLAttributeEquivalent(
    CSSPropertyID id,
    const HTMLQualifiedName& tag_name,
    const QualifiedName& attr_name)
    : HTMLElementEquivalent(id, tag_name), attr_name_(attr_name) {}

HTMLAttributeEquivalent::HTMLAttributeEquivalent(CSSPropertyID id,
                                                 const QualifiedName& attr_name)
    : HTMLElementEquivalent(id), attr_name_(attr_name) {}

bool HTMLAttributeEquivalent::ValueIsPresentInStyle(
    HTMLElement* element,
    CSSPropertyValueSet* style) const {
  const CSSValue* value = AttributeValueAsCSSValue(element);
  const CSSValue* style_value = style->GetPropertyCSSValue(property_id_);

  return base::ValuesEquivalent(value, style_value);
}

void HTMLAttributeEquivalent::AddToStyle(Element* element,
                                         EditingStyle* style) const {
  if (const CSSValue* value = AttributeValueAsCSSValue(element)) {
    style->SetProperty(property_id_, value->CssText(), /* important */ false,
                       element->GetExecutionContext()->GetSecureContextMode());
  }
}

const CSSValue* HTMLAttributeEquivalent::AttributeValueAsCSSValue(
    Element* element) const {
  DCHECK(element);
  const AtomicString& value = element->getAttribute(attr_name_);
  if (value.IsNull())
    return nullptr;

  auto* dummy_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  dummy_style->ParseAndSetProperty(
      property_id_, value, /* important */ false,
      element->GetExecutionContext()->GetSecureContextMode());
  return dummy_style->GetPropertyCSSValue(property_id_);
}

class HTMLFontSizeEquivalent final : public HTMLAttributeEquivalent {
 public:
  static HTMLFontSizeEquivalent* Create() {
    return MakeGarbageCollected<HTMLFontSizeEquivalent>();
  }

  HTMLFontSizeEquivalent();

  const CSSValue* AttributeValueAsCSSValue(Element*) const override;

  void Trace(Visitor* visitor) const override {
    HTMLAttributeEquivalent::Trace(visitor);
  }
};

HTMLFontSizeEquivalent::HTMLFontSizeEquivalent()
    : HTMLAttributeEquivalent(CSSPropertyID::kFontSize,
                              html_names::kFontTag,
                              html_names::kSizeAttr) {}

const CSSValue* HTMLFontSizeEquivalent::AttributeValueAsCSSValue(
    Element* element) const {
  DCHECK(element);
  const AtomicString& value = element->getAttribute(attr_name_);
  if (value.IsNull())
    return nullptr;
  CSSValueID size;
  if (!HTMLFontElement::CssValueFromFontSizeNumber(value, size))
    return nullptr;
  return CSSIdentifierValue::Create(size);
}

EditingStyle::EditingStyle(Element* element,
                           PropertiesToInclude properties_to_include) {
  Init(element, properties_to_include);
}

EditingStyle::EditingStyle(const Position& position,
                           PropertiesToInclude properties_to_include) {
  Init(position.AnchorNode(), properties_to_include);
}

EditingStyle::EditingStyle(const CSSPropertyValueSet* style)
    : mutable_style_(style ? style->MutableCopy() : nullptr) {
  ExtractFontSizeDelta();
}

EditingStyle::EditingStyle(CSSPropertyID property_id,
                           const String& value,
                           SecureContextMode secure_context_mode)
    : mutable_style_(nullptr) {
  SetProperty(property_id, value, /* important */ false, secure_context_mode);
  is_vertical_align_ = property_id == CSSPropertyID::kVerticalAlign &&
                       (value == "sub" || value == "super");
}

static Color CssValueToColor(const CSSValue* value) {
  if (!value)
    return Color::kTransparent;

  auto* color_value = DynamicTo<cssvalue::CSSColor>(value);
  if (!color_value && !value->IsPrimitiveValue() && !value->IsIdentifierValue())
    return Color::kTransparent;

  if (color_value)
    return color_value->Value();

  Color color = Color::kTransparent;
  // FIXME: Why ignore the return value?
  CSSParser::ParseColor(color, value->CssText());
  return color;
}

static inline Color GetFontColor(CSSStyleDeclaration* style) {
  return CssValueToColor(
      style->GetPropertyCSSValueInternal(CSSPropertyID::kColor));
}

static inline Color GetFontColor(CSSPropertyValueSet* style) {
  return CssValueToColor(style->GetPropertyCSSValue(CSSPropertyID::kColor));
}

static inline Color GetBackgroundColor(CSSStyleDeclaration* style) {
  return CssValueToColor(
      style->GetPropertyCSSValueInternal(CSSPropertyID::kBackgroundColor));
}

static inline Color GetBackgroundColor(CSSPropertyValueSet* style) {
  return CssValueToColor(
      style->GetPropertyCSSValue(CSSPropertyID::kBackgroundColor));
}

static inline Color BackgroundColorInEffect(Node* node) {
  return CssValueToColor(
      EditingStyleUtilities::BackgroundColorValueInEffect(node));
}

static CSSValueID NormalizeTextAlign(CSSValueID text_align) {
  switch (text_align) {
    case CSSValueID::kCenter:
    case CSSValueID::kWebkitCenter:
      return CSSValueID::kCenter;
    case CSSValueID::kJustify:
      return CSSValueID::kJustify;
    case CSSValueID::kLeft:
    case CSSValueID::kWebkitLeft:
      return CSSValueID::kLeft;
    case CSSValueID::kRight:
    case CSSValueID::kWebkitRight:
      return CSSValueID::kRight;
    case CSSValueID::kStart:
    case CSSValueID::kEnd:
      return text_align;
    default:
      return CSSValueID::kInvalid;
  }
}
static CSSValueID TextAlignResolvingStartAndEnd(CSSValueID text_align,
                                                TextDirection direction) {
  const CSSValueID normalized = NormalizeTextAlign(text_align);
  switch (normalized) {
    case CSSValueID::kStart:
      return IsLtr(direction) ? CSSValueID::kLeft : CSSValueID::kRight;
    case CSSValueID::kEnd:
      return IsLtr(direction) ? CSSValueID::kRight : CSSValueID::kLeft;
    default:
      return normalized;
  }
}

// Returns true "text-align" property of |style| is redundant when applying
// |style| inheriting from |base_style| to |node|.
// Note: direction for "text-align:start" and "text-align:end" are taken
// from |node|.
template <typename T>
static bool IsRedundantTextAlign(MutableCSSPropertyValueSet* style,
                                 T* base_style,
                                 Node* node) {
  DCHECK(node);
  const CSSValueID base_text_align = NormalizeTextAlign(
      GetIdentifierValue(base_style, CSSPropertyID::kTextAlign));
  if (base_text_align == CSSValueID::kInvalid)
    return false;
  const CSSValueID text_align =
      NormalizeTextAlign(GetIdentifierValue(style, CSSPropertyID::kTextAlign));
  if (text_align == CSSValueID::kInvalid)
    return false;
  if (text_align == base_text_align)
    return true;
  const ComputedStyle* node_style = node->GetComputedStyle();
  if (!node_style) {
    return true;
  }
  TextDirection node_direction = node_style->Direction();
  if (base_text_align == CSSValueID::kStart ||
      base_text_align == CSSValueID::kEnd) {
    // Returns true for "text-align:left" of <p>
    //   <div style="text-align:start"><p dir="ltr" style="text-align:left">
    // because meaning of "text-align:start" in <p> is identical to
    // "text-align:left".
    //
    // Returns false for "text-align:left" of <p>
    //   <div style="text-align:start"><p dir="rtl" style="text-align:left">
    // because meaning of "text-align:start" in <p> is identical to
    // "text-align:right".
    return TextAlignResolvingStartAndEnd(base_text_align, node_direction) ==
           text_align;
  }
  if (text_align == CSSValueID::kStart || text_align == CSSValueID::kEnd) {
    // Returns true for "text-align:start" of <p>
    //  <div style="text-align:left"><p dir="ltr" style="text-align:start">
    //  <div style="text-align:right"><p dir="rtl" style="text-align:start">
    // Returns false for "text-align:start" of <p>
    //  <div style="text-align:left"><p dir="rtl" style="text-align:start">
    //  <div style="text-align:right"><p dir="ltr" style="text-align:start">
    return TextAlignResolvingStartAndEnd(text_align, node_direction) ==
           base_text_align;
  }
  return false;
}

namespace {

Element* ElementFromStyledNode(Node* node) {
  if (Element* element = DynamicTo<Element>(node)) {
    return element;
  }
  if (node) {
    // This should probably be FlatTreeTraversal::ParentElement() instead, but
    // it breaks tests.
    return node->ParentOrShadowHostElement();
  }
  return nullptr;
}

}  // namespace

void EditingStyle::Init(Node* node, PropertiesToInclude properties_to_include) {
  if (IsTabHTMLSpanElementTextNode(node))
    node = TabSpanElement(node)->parentNode();
  else if (IsTabHTMLSpanElement(node))
    node = node->parentNode();
  node_ = node;
  auto* computed_style_at_position =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(
          ElementFromStyledNode(node));
  mutable_style_ =
      properties_to_include == kAllProperties && computed_style_at_position
          ? computed_style_at_position->CopyProperties()
          : CopyEditingProperties(node ? node->GetExecutionContext() : nullptr,
                                  computed_style_at_position);

  if (properties_to_include == kEditingPropertiesInEffect) {
    if (const CSSValue* value =
            EditingStyleUtilities::BackgroundColorValueInEffect(node)) {
      mutable_style_->ParseAndSetProperty(
          CSSPropertyID::kBackgroundColor, value->CssText(),
          /* important */ false,
          node->GetExecutionContext()->GetSecureContextMode());
    }
    if (const CSSValue* value = computed_style_at_position->GetPropertyCSSValue(
            CSSPropertyID::kWebkitTextDecorationsInEffect)) {
      mutable_style_->ParseAndSetProperty(
          CSSPropertyID::kTextDecoration, value->CssText(),
          /* important */ false,
          node->GetExecutionContext()->GetSecureContextMode());
    }
  }

  const ComputedStyle* computed_style =
      node ? node->GetComputedStyle() : nullptr;
  if (computed_style) {
    // Fix for crbug.com/768261: due to text-autosizing, reading the current
    // computed font size and re-writing it to an element may actually cause the
    // font size to become larger (since the autosizer will run again on the new
    // computed size). The fix is to toss out the computed size property here
    // and use ComputedStyle::SpecifiedFontSize().
    if (computed_style->ComputedFontSize() !=
        computed_style->SpecifiedFontSize()) {
      // ReplaceSelectionCommandTest_TextAutosizingDoesntInflateText gets here.
      mutable_style_->ParseAndSetProperty(
          CSSPropertyID::kFontSize,
          CSSNumericLiteralValue::Create(computed_style->SpecifiedFontSize(),
                                         CSSPrimitiveValue::UnitType::kPixels)
              ->CssText(),
          /* important */ false,
          node->GetExecutionContext()->GetSecureContextMode());
    }
    RemoveForcedColorsIfNeeded(computed_style);
    RemoveInheritedColorsIfNeeded(computed_style);
    ReplaceFontSizeByKeywordIfPossible(
        computed_style, node->GetExecutionContext()->GetSecureContextMode(),
        computed_style_at_position);
  }

  is_monospace_font_ = computed_style_at_position->IsMonospaceFont();
  ExtractFontSizeDelta();
}

void EditingStyle::RemoveForcedColorsIfNeeded(
    const ComputedStyle* computed_style) {
  if (!computed_style->InForcedColorsMode()) {
    return;
  }
  mutable_style_->RemoveProperty(CSSPropertyID::kColor);
  mutable_style_->RemoveProperty(CSSPropertyID::kBackgroundColor);
  mutable_style_->RemoveProperty(CSSPropertyID::kTextDecorationColor);
}

void EditingStyle::RemoveInheritedColorsIfNeeded(
    const ComputedStyle* computed_style) {
  // If a node's text fill color is currentColor, then its children use
  // their font-color as their text fill color (they don't
  // inherit it).  Likewise for stroke color.
  // Similar thing happens for caret-color if it's auto or currentColor.
  if (computed_style->TextFillColor().IsCurrentColor())
    mutable_style_->RemoveProperty(CSSPropertyID::kWebkitTextFillColor);
  if (computed_style->TextStrokeColor().IsCurrentColor())
    mutable_style_->RemoveProperty(CSSPropertyID::kWebkitTextStrokeColor);
  if (computed_style->CaretColor().IsAutoColor() ||
      computed_style->CaretColor().IsCurrentColor())
    mutable_style_->RemoveProperty(CSSPropertyID::kCaretColor);
}

CSSValueID EditingStyle::GetProperty(CSSPropertyID property_id) const {
  return GetIdentifierValue(mutable_style_.Get(), property_id);
}

void EditingStyle::SetProperty(CSSPropertyID property_id,
                               const String& value,
                               bool important,
                               SecureContextMode secure_context_mode) {
  if (!mutable_style_) {
    mutable_style_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  }

  mutable_style_->ParseAndSetProperty(property_id, value, important,
                                      secure_context_mode);
}

void EditingStyle::ReplaceFontSizeByKeywordIfPossible(
    const ComputedStyle* computed_style,
    SecureContextMode secure_context_mode,
    CSSComputedStyleDeclaration* css_computed_style) {
  DCHECK(computed_style);
  if (computed_style->GetFontDescription().KeywordSize()) {
    if (const CSSValue* keyword =
            css_computed_style->GetFontSizeCSSValuePreferringKeyword()) {
      mutable_style_->ParseAndSetProperty(
          CSSPropertyID::kFontSize, keyword->CssText(),
          /* important */ false, secure_context_mode);
    }
  }
}

void EditingStyle::ExtractFontSizeDelta() {
  if (!mutable_style_)
    return;

  if (mutable_style_->GetPropertyCSSValue(CSSPropertyID::kFontSize)) {
    // Explicit font size overrides any delta.
    mutable_style_->RemoveProperty(CSSPropertyID::kInternalFontSizeDelta);
    return;
  }

  // Get the adjustment amount out of the style.
  const CSSValue* value = mutable_style_->GetPropertyCSSValue(
      CSSPropertyID::kInternalFontSizeDelta);
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return;

  // Only PX handled now. If we handle more types in the future, perhaps
  // a switch statement here would be more appropriate.
  if (!primitive_value->IsPx())
    return;

  font_size_delta_ = primitive_value->GetFloatValue();
  mutable_style_->RemoveProperty(CSSPropertyID::kInternalFontSizeDelta);
}

bool EditingStyle::IsEmpty() const {
  return (!mutable_style_ || mutable_style_->IsEmpty()) &&
         font_size_delta_ == kNoFontDelta;
}

bool EditingStyle::GetTextDirection(
    mojo_base::mojom::blink::TextDirection& writing_direction) const {
  if (!mutable_style_)
    return false;

  const CSSValue* unicode_bidi =
      mutable_style_->GetPropertyCSSValue(CSSPropertyID::kUnicodeBidi);
  auto* unicode_bidi_identifier_value =
      DynamicTo<CSSIdentifierValue>(unicode_bidi);
  if (!unicode_bidi_identifier_value)
    return false;

  CSSValueID unicode_bidi_value = unicode_bidi_identifier_value->GetValueID();
  if (EditingStyleUtilities::IsEmbedOrIsolate(unicode_bidi_value)) {
    const CSSValue* direction =
        mutable_style_->GetPropertyCSSValue(CSSPropertyID::kDirection);
    auto* direction_identifier_value = DynamicTo<CSSIdentifierValue>(direction);
    if (!direction_identifier_value)
      return false;

    writing_direction =
        direction_identifier_value->GetValueID() == CSSValueID::kLtr
            ? mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT
            : mojo_base::mojom::blink::TextDirection::RIGHT_TO_LEFT;

    return true;
  }

  if (unicode_bidi_value == CSSValueID::kNormal) {
    writing_direction =
        mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;
    return true;
  }

  return false;
}

void EditingStyle::OverrideWithStyle(const CSSPropertyValueSet* style) {
  if (!style || style->IsEmpty())
    return;
  if (!mutable_style_) {
    mutable_style_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  }
  mutable_style_->MergeAndOverrideOnConflict(style);
  ExtractFontSizeDelta();
}

void EditingStyle::Clear() {
  mutable_style_.Clear();
  is_monospace_font_ = false;
  font_size_delta_ = kNoFontDelta;
}

EditingStyle* EditingStyle::Copy() const {
  EditingStyle* copy = MakeGarbageCollected<EditingStyle>();
  if (mutable_style_)
    copy->mutable_style_ = mutable_style_->MutableCopy();
  copy->is_monospace_font_ = is_monospace_font_;
  copy->font_size_delta_ = font_size_delta_;
  return copy;
}

// This is the list of CSS properties that apply specially to block-level
// elements.
static const CSSPropertyID kStaticBlockProperties[] = {
    CSSPropertyID::kBreakAfter, CSSPropertyID::kBreakBefore,
    CSSPropertyID::kBreakInside, CSSPropertyID::kOrphans,
    CSSPropertyID::kOverflow,  // This can be also be applied to replaced
                               // elements
    CSSPropertyID::kColumnCount, CSSPropertyID::kColumnGap,
    CSSPropertyID::kColumnRuleColor, CSSPropertyID::kColumnRuleStyle,
    CSSPropertyID::kColumnRuleWidth, CSSPropertyID::kWebkitColumnBreakBefore,
    CSSPropertyID::kWebkitColumnBreakAfter,
    CSSPropertyID::kWebkitColumnBreakInside, CSSPropertyID::kColumnWidth,
    CSSPropertyID::kPageBreakAfter, CSSPropertyID::kPageBreakBefore,
    CSSPropertyID::kPageBreakInside, CSSPropertyID::kTextAlign,
    CSSPropertyID::kTextAlignLast, CSSPropertyID::kTextIndent,
    CSSPropertyID::kWidows};

static const Vector<const CSSProperty*>& BlockPropertiesVector(
    const ExecutionContext* execution_context) {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  if (properties.empty()) {
    CSSProperty::FilterWebExposedCSSPropertiesIntoVector(
        execution_context, kStaticBlockProperties,
        std::size(kStaticBlockProperties), properties);
  }
  return properties;
}

EditingStyle* EditingStyle::ExtractAndRemoveBlockProperties(
    const ExecutionContext* execution_context) {
  EditingStyle* block_properties = MakeGarbageCollected<EditingStyle>();
  if (!mutable_style_)
    return block_properties;

  block_properties->mutable_style_ = mutable_style_->CopyPropertiesInSet(
      BlockPropertiesVector(execution_context));
  RemoveBlockProperties(execution_context);

  return block_properties;
}

EditingStyle* EditingStyle::ExtractAndRemoveTextDirection(
    SecureContextMode secure_context_mode) {
  EditingStyle* text_direction = MakeGarbageCollected<EditingStyle>();
  text_direction->mutable_style_ =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  text_direction->mutable_style_->SetLonghandProperty(
      CSSPropertyID::kUnicodeBidi, CSSValueID::kIsolate,
      mutable_style_->PropertyIsImportant(CSSPropertyID::kUnicodeBidi));

  text_direction->mutable_style_->ParseAndSetProperty(
      CSSPropertyID::kDirection,
      mutable_style_->GetPropertyValue(CSSPropertyID::kDirection),
      mutable_style_->PropertyIsImportant(CSSPropertyID::kDirection),
      secure_context_mode);

  mutable_style_->RemoveProperty(CSSPropertyID::kUnicodeBidi);
  mutable_style_->RemoveProperty(CSSPropertyID::kDirection);

  return text_direction;
}

void EditingStyle::RemoveBlockProperties(
    const ExecutionContext* execution_context) {
  if (!mutable_style_)
    return;

  mutable_style_->RemovePropertiesInSet(
      BlockPropertiesVector(execution_context));
}

void EditingStyle::RemoveStyleAddedByElement(Element* element) {
  if (!element || !element->parentElement()) {
    return;
  }
  MutableCSSPropertyValueSet* parent_style =
      CopyEditingProperties(element->parentElement()->GetExecutionContext(),
                            MakeGarbageCollected<CSSComputedStyleDeclaration>(
                                element->parentElement()),
                            kAllEditingProperties);
  MutableCSSPropertyValueSet* element_style = CopyEditingProperties(
      element->GetExecutionContext(),
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element),
      kAllEditingProperties);
  element_style->RemoveEquivalentProperties(parent_style);
  mutable_style_->RemoveEquivalentProperties(element_style);
}

void EditingStyle::RemoveStyleConflictingWithStyleOfElement(Element* element) {
  if (!element || !element->parentElement() || !mutable_style_) {
    return;
  }

  MutableCSSPropertyValueSet* parent_style =
      CopyEditingProperties(element->parentElement()->GetExecutionContext(),
                            MakeGarbageCollected<CSSComputedStyleDeclaration>(
                                element->parentElement()),
                            kAllEditingProperties);
  MutableCSSPropertyValueSet* element_style = CopyEditingProperties(
      element->GetExecutionContext(),
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element),
      kAllEditingProperties);
  element_style->RemoveEquivalentProperties(parent_style);

  unsigned property_count = element_style->PropertyCount();
  for (unsigned i = 0; i < property_count; ++i)
    mutable_style_->RemoveProperty(element_style->PropertyAt(i).Id());
}

void EditingStyle::CollapseTextDecorationProperties(
    SecureContextMode secure_context_mode) {
  if (!mutable_style_)
    return;

  const CSSValue* text_decorations_in_effect =
      mutable_style_->GetPropertyCSSValue(
          CSSPropertyID::kWebkitTextDecorationsInEffect);
  if (!text_decorations_in_effect)
    return;

  if (text_decorations_in_effect->IsValueList()) {
    mutable_style_->ParseAndSetProperty(
        CSSPropertyID::kTextDecorationLine,
        text_decorations_in_effect->CssText(),
        mutable_style_->PropertyIsImportant(CSSPropertyID::kTextDecorationLine),
        secure_context_mode);
  } else {
    mutable_style_->RemoveProperty(CSSPropertyID::kTextDecorationLine);
  }
  mutable_style_->RemoveProperty(CSSPropertyID::kWebkitTextDecorationsInEffect);
}

EditingTriState EditingStyle::TriStateOfStyle(
    ExecutionContext* execution_context,
    EditingStyle* style,
    SecureContextMode secure_context_mode) const {
  if (!style || !style->mutable_style_)
    return EditingTriState::kFalse;
  DCHECK(style->node_);
  return TriStateOfStyle(
      style->mutable_style_->EnsureCSSStyleDeclaration(execution_context),
      style->node_, kDoNotIgnoreTextOnlyProperties, secure_context_mode);
}

EditingTriState EditingStyle::TriStateOfStyle(
    CSSStyleDeclaration* style_to_compare,
    Node* node,
    ShouldIgnoreTextOnlyProperties should_ignore_text_only_properties,
    SecureContextMode secure_context_mode) const {
  // editing/execCommand/query-text-alignment.html requires |node|.
  DCHECK(node);
  MutableCSSPropertyValueSet* difference = GetPropertiesNotIn(
      mutable_style_.Get(), node, style_to_compare, secure_context_mode);

  // CSS properties that create a visual difference only when applied to text.
  static const CSSProperty* kTextOnlyProperties[] = {
      // FIXME: CSSPropertyID::kTextDecoration needs to be removed when CSS3
      // Text
      // Decoration feature is no longer experimental.
      &GetCSSPropertyTextDecoration(),
      &GetCSSPropertyTextDecorationLine(),
      &GetCSSPropertyWebkitTextDecorationsInEffect(),
      &GetCSSPropertyFontStyle(),
      &GetCSSPropertyFontWeight(),
      &GetCSSPropertyColor(),
  };
  if (should_ignore_text_only_properties == kIgnoreTextOnlyProperties) {
    difference->RemovePropertiesInSet(kTextOnlyProperties);
  }

  if (difference->IsEmpty())
    return EditingTriState::kTrue;
  if (difference->PropertyCount() == mutable_style_->PropertyCount())
    return EditingTriState::kFalse;

  return EditingTriState::kMixed;
}

EditingTriState EditingStyle::TriStateOfStyle(
    const VisibleSelection& selection,
    SecureContextMode secure_context_mode) const {
  if (selection.IsNone())
    return EditingTriState::kFalse;

  if (selection.IsCaret()) {
    return TriStateOfStyle(
        selection.Start().AnchorNode()->GetExecutionContext(),
        EditingStyleUtilities::CreateStyleAtSelectionStart(selection),
        secure_context_mode);
  }

  EditingTriState state = EditingTriState::kFalse;
  bool node_is_start = true;
  for (Node& node : NodeTraversal::StartsAt(*selection.Start().AnchorNode())) {
    if (node.GetLayoutObject() && IsEditable(node)) {
      auto* computed_style = MakeGarbageCollected<CSSComputedStyleDeclaration>(
          ElementFromStyledNode(&node));
      CSSStyleDeclaration* node_style = computed_style;
      if (computed_style) {
        // If the selected element has <sub> or <sup> ancestor element, apply
        // the corresponding style(vertical-align) to it so that
        // document.queryCommandState() works with the style. See bug
        // http://crbug.com/582225.
        if (is_vertical_align_ &&
            GetIdentifierValue(computed_style, CSSPropertyID::kVerticalAlign) ==
                CSSValueID::kBaseline) {
          const auto* vertical_align =
              To<CSSIdentifierValue>(mutable_style_->GetPropertyCSSValue(
                  CSSPropertyID::kVerticalAlign));
          if (EditingStyleUtilities::HasAncestorVerticalAlignStyle(
                  node, vertical_align->GetValueID())) {
            auto* mutable_style = computed_style->CopyProperties();
            mutable_style->SetProperty(CSSPropertyID::kVerticalAlign,
                                       *vertical_align);
            node_style = mutable_style->EnsureCSSStyleDeclaration(
                node.GetExecutionContext());
          }
        }

        // Pass EditingStyle::DoNotIgnoreTextOnlyProperties without checking if
        // node.isTextNode() because the node can be an element node. See bug
        // http://crbug.com/584939.
        EditingTriState node_state = TriStateOfStyle(
            node_style, &node, EditingStyle::kDoNotIgnoreTextOnlyProperties,
            secure_context_mode);
        if (node_is_start) {
          state = node_state;
          node_is_start = false;
        } else if (state != node_state && node.IsTextNode()) {
          state = EditingTriState::kMixed;
          break;
        }
      }
    }
    if (&node == selection.End().AnchorNode())
      break;
  }

  return state;
}

bool EditingStyle::ConflictsWithInlineStyleOfElement(
    HTMLElement* element,
    EditingStyle* extracted_style,
    Vector<CSSPropertyID>* conflicting_properties) const {
  DCHECK(element);
  DCHECK(!conflicting_properties || conflicting_properties->empty());

  const CSSPropertyValueSet* inline_style = element->InlineStyle();
  if (!mutable_style_ || !inline_style)
    return false;

  unsigned property_count = mutable_style_->PropertyCount();
  for (unsigned i = 0; i < property_count; ++i) {
    CSSPropertyID property_id = mutable_style_->PropertyAt(i).Id();

    // We don't override `white-space-collapse` property of a tab span because
    // that would collapse the tab into a space.
    //
    // Logically speaking, only `white-space-collapse` is needed (i.e.,
    // `text-wrap` is not needed.) But including other longhands helps producing
    // `white-space` instead of `white-space-collapse`. Because the snippet
    // produced by this logic may be sent to other browsers by copy&paste,
    // e-mail, etc., `white-space` is more interoperable when
    // `white-space-collapse` is not broadly supported. See crbug.com/1417543
    // and `editing/pasteboard/pasting-tabs.html`.
#if EXPENSIVE_DCHECKS_ARE_ON()
    DCHECK_NE(property_id, CSSPropertyID::kWhiteSpace);
    DCHECK_EQ(whiteSpaceShorthand().length(), 2u);
    DCHECK_EQ(whiteSpaceShorthand().properties()[0]->PropertyID(),
              CSSPropertyID::kWhiteSpaceCollapse);
    DCHECK_EQ(whiteSpaceShorthand().properties()[1]->PropertyID(),
              CSSPropertyID::kTextWrapMode);
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
    const bool is_whitespace_property =
        property_id == CSSPropertyID::kWhiteSpaceCollapse ||
        property_id == CSSPropertyID::kTextWrapMode;
    if (is_whitespace_property && IsTabHTMLSpanElement(element)) {
      continue;
    }

    if (property_id == CSSPropertyID::kWebkitTextDecorationsInEffect &&
        inline_style->GetPropertyCSSValue(CSSPropertyID::kTextDecorationLine)) {
      if (!conflicting_properties)
        return true;
      conflicting_properties->push_back(CSSPropertyID::kTextDecoration);
      // Because text-decoration expands to text-decoration-line,
      // we also state it as conflicting.
      conflicting_properties->push_back(CSSPropertyID::kTextDecorationLine);
      if (extracted_style) {
        extracted_style->SetProperty(
            CSSPropertyID::kTextDecorationLine,
            inline_style->GetPropertyValue(CSSPropertyID::kTextDecorationLine),
            inline_style->PropertyIsImportant(
                CSSPropertyID::kTextDecorationLine),
            element->GetExecutionContext()->GetSecureContextMode());
      }
      continue;
    }

    if (!inline_style->GetPropertyCSSValue(property_id))
      continue;

    if (property_id == CSSPropertyID::kUnicodeBidi &&
        inline_style->GetPropertyCSSValue(CSSPropertyID::kDirection)) {
      if (!conflicting_properties)
        return true;
      conflicting_properties->push_back(CSSPropertyID::kDirection);
      if (extracted_style) {
        extracted_style->SetProperty(
            property_id, inline_style->GetPropertyValue(property_id),
            inline_style->PropertyIsImportant(property_id),
            element->GetExecutionContext()->GetSecureContextMode());
      }
    }

    if (!conflicting_properties)
      return true;

    conflicting_properties->push_back(property_id);

    if (extracted_style) {
      extracted_style->SetProperty(
          property_id, inline_style->GetPropertyValue(property_id),
          inline_style->PropertyIsImportant(property_id),
          element->GetExecutionContext()->GetSecureContextMode());
    }
  }

  return conflicting_properties && !conflicting_properties->empty();
}

static const HeapVector<Member<HTMLElementEquivalent>>&
HtmlElementEquivalents() {
  DEFINE_STATIC_LOCAL(
      Persistent<HeapVector<Member<HTMLElementEquivalent>>>,
      html_element_equivalents,
      (MakeGarbageCollected<HeapVector<Member<HTMLElementEquivalent>>>()));
  if (!html_element_equivalents->size()) {
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(
            CSSPropertyID::kFontWeight, CSSValueID::kBold, html_names::kBTag));
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(CSSPropertyID::kFontWeight,
                                                    CSSValueID::kBold,
                                                    html_names::kStrongTag));
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(
            CSSPropertyID::kVerticalAlign, CSSValueID::kSub,
            html_names::kSubTag));
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(
            CSSPropertyID::kVerticalAlign, CSSValueID::kSuper,
            html_names::kSupTag));
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(
            CSSPropertyID::kFontStyle, CSSValueID::kItalic, html_names::kITag));
    html_element_equivalents->push_back(
        MakeGarbageCollected<HTMLElementEquivalent>(CSSPropertyID::kFontStyle,
                                                    CSSValueID::kItalic,
                                                    html_names::kEmTag));

    html_element_equivalents->push_back(HTMLTextDecorationEquivalent::Create(
        CSSValueID::kUnderline, html_names::kUTag));
    html_element_equivalents->push_back(HTMLTextDecorationEquivalent::Create(
        CSSValueID::kLineThrough, html_names::kSTag));
    html_element_equivalents->push_back(HTMLTextDecorationEquivalent::Create(
        CSSValueID::kLineThrough, html_names::kStrikeTag));
  }

  return *html_element_equivalents;
}

bool EditingStyle::ConflictsWithImplicitStyleOfElement(
    HTMLElement* element,
    EditingStyle* extracted_style,
    ShouldExtractMatchingStyle should_extract_matching_style) const {
  if (!mutable_style_)
    return false;

  const HeapVector<Member<HTMLElementEquivalent>>& html_element_equivalents =
      HtmlElementEquivalents();
  for (wtf_size_t i = 0; i < html_element_equivalents.size(); ++i) {
    const HTMLElementEquivalent* equivalent = html_element_equivalents[i].Get();
    if (equivalent->Matches(element) &&
        equivalent->PropertyExistsInStyle(mutable_style_.Get()) &&
        (should_extract_matching_style == kExtractMatchingStyle ||
         !equivalent->ValueIsPresentInStyle(element, mutable_style_.Get()))) {
      if (extracted_style)
        equivalent->AddToStyle(element, extracted_style);
      return true;
    }
  }
  return false;
}

static const HeapVector<Member<HTMLAttributeEquivalent>>&
HtmlAttributeEquivalents() {
  DEFINE_STATIC_LOCAL(
      Persistent<HeapVector<Member<HTMLAttributeEquivalent>>>,
      html_attribute_equivalents,
      (MakeGarbageCollected<HeapVector<Member<HTMLAttributeEquivalent>>>()));
  if (!html_attribute_equivalents->size()) {
    // elementIsStyledSpanOrHTMLEquivalent depends on the fact each
    // HTMLAttriuteEquivalent matches exactly one attribute of exactly one
    // element except dirAttr.
    html_attribute_equivalents->push_back(
        MakeGarbageCollected<HTMLAttributeEquivalent>(CSSPropertyID::kColor,
                                                      html_names::kFontTag,
                                                      html_names::kColorAttr));
    html_attribute_equivalents->push_back(
        MakeGarbageCollected<HTMLAttributeEquivalent>(
            CSSPropertyID::kFontFamily, html_names::kFontTag,
            html_names::kFaceAttr));
    html_attribute_equivalents->push_back(HTMLFontSizeEquivalent::Create());

    html_attribute_equivalents->push_back(
        MakeGarbageCollected<HTMLAttributeEquivalent>(CSSPropertyID::kDirection,
                                                      html_names::kDirAttr));
    html_attribute_equivalents->push_back(
        MakeGarbageCollected<HTMLAttributeEquivalent>(
            CSSPropertyID::kUnicodeBidi, html_names::kDirAttr));
  }

  return *html_attribute_equivalents;
}

bool EditingStyle::ConflictsWithImplicitStyleOfAttributes(
    HTMLElement* element) const {
  DCHECK(element);
  if (!mutable_style_)
    return false;

  const HeapVector<Member<HTMLAttributeEquivalent>>&
      html_attribute_equivalents = HtmlAttributeEquivalents();
  for (const auto& equivalent : html_attribute_equivalents) {
    if (equivalent->Matches(element) &&
        equivalent->PropertyExistsInStyle(mutable_style_.Get()) &&
        !equivalent->ValueIsPresentInStyle(element, mutable_style_.Get()))
      return true;
  }

  return false;
}

bool EditingStyle::ExtractConflictingImplicitStyleOfAttributes(
    HTMLElement* element,
    ShouldPreserveWritingDirection should_preserve_writing_direction,
    EditingStyle* extracted_style,
    Vector<QualifiedName>& conflicting_attributes,
    ShouldExtractMatchingStyle should_extract_matching_style) const {
  DCHECK(element);
  // HTMLAttributeEquivalent::addToStyle doesn't support unicode-bidi and
  // direction properties
  if (extracted_style)
    DCHECK_EQ(should_preserve_writing_direction, kPreserveWritingDirection);
  if (!mutable_style_)
    return false;

  const HeapVector<Member<HTMLAttributeEquivalent>>&
      html_attribute_equivalents = HtmlAttributeEquivalents();
  bool removed = false;
  for (const auto& attribute : html_attribute_equivalents) {
    const HTMLAttributeEquivalent* equivalent = attribute.Get();

    // unicode-bidi and direction are pushed down separately so don't push down
    // with other styles.
    if (should_preserve_writing_direction == kPreserveWritingDirection &&
        equivalent->AttributeName() == html_names::kDirAttr)
      continue;

    if (!equivalent->Matches(element) ||
        !equivalent->PropertyExistsInStyle(mutable_style_.Get()) ||
        (should_extract_matching_style == kDoNotExtractMatchingStyle &&
         equivalent->ValueIsPresentInStyle(element, mutable_style_.Get())))
      continue;

    if (extracted_style)
      equivalent->AddToStyle(element, extracted_style);
    conflicting_attributes.push_back(equivalent->AttributeName());
    removed = true;
  }

  return removed;
}

bool EditingStyle::StyleIsPresentInComputedStyleOfNode(Node* node) const {
  return !mutable_style_ ||
         GetPropertiesNotIn(mutable_style_.Get(), node,
                            MakeGarbageCollected<CSSComputedStyleDeclaration>(
                                ElementFromStyledNode(node)),
                            node->GetExecutionContext()->GetSecureContextMode())
             ->IsEmpty();
}

bool EditingStyle::ElementIsStyledSpanOrHTMLEquivalent(
    const HTMLElement* element) {
  DCHECK(element);
  bool element_is_span_or_element_equivalent = false;
  if (IsA<HTMLSpanElement>(*element)) {
    element_is_span_or_element_equivalent = true;
  } else {
    const HeapVector<Member<HTMLElementEquivalent>>& html_element_equivalents =
        HtmlElementEquivalents();
    wtf_size_t i;
    for (i = 0; i < html_element_equivalents.size(); ++i) {
      if (html_element_equivalents[i]->Matches(element)) {
        element_is_span_or_element_equivalent = true;
        break;
      }
    }
  }

  AttributeCollection attributes = element->Attributes();
  if (attributes.IsEmpty()) {
    // span, b, etc... without any attributes
    return element_is_span_or_element_equivalent;
  }

  unsigned matched_attributes = 0;
  const HeapVector<Member<HTMLAttributeEquivalent>>&
      html_attribute_equivalents = HtmlAttributeEquivalents();
  for (const auto& equivalent : html_attribute_equivalents) {
    if (equivalent->Matches(element) &&
        equivalent->AttributeName() != html_names::kDirAttr)
      matched_attributes++;
  }

  if (!element_is_span_or_element_equivalent && !matched_attributes) {
    // element is not a span, a html element equivalent, or font element.
    return false;
  }

  if (element->hasAttribute(html_names::kStyleAttr)) {
    if (const CSSPropertyValueSet* style = element->InlineStyle()) {
      unsigned property_count = style->PropertyCount();
      for (unsigned i = 0; i < property_count; ++i) {
        if (!IsEditingProperty(element->GetExecutionContext(),
                               style->PropertyAt(i).Id()))
          return false;
      }
    }
    matched_attributes++;
  }

  // font with color attribute, span with style attribute, etc...
  DCHECK_LE(matched_attributes, attributes.size());
  return matched_attributes >= attributes.size();
}

void EditingStyle::PrepareToApplyAt(
    const Position& position,
    ShouldPreserveWritingDirection should_preserve_writing_direction) {
  if (!mutable_style_)
    return;
  DCHECK(position.IsNotNull());

  // ReplaceSelectionCommand::handleStyleSpans() requires that this function
  // only removes the editing style. If this function was modified in the future
  // to delete all redundant properties, then add a boolean value to indicate
  // which one of editingStyleAtPosition or computedStyle is called.
  EditingStyle* editing_style_at_position =
      MakeGarbageCollected<EditingStyle>(position, kEditingPropertiesInEffect);
  CSSPropertyValueSet* style_at_position =
      editing_style_at_position->mutable_style_.Get();

  const CSSValue* unicode_bidi = nullptr;
  const CSSValue* direction = nullptr;
  if (should_preserve_writing_direction == kPreserveWritingDirection) {
    unicode_bidi =
        mutable_style_->GetPropertyCSSValue(CSSPropertyID::kUnicodeBidi);
    direction = mutable_style_->GetPropertyCSSValue(CSSPropertyID::kDirection);
  }

  mutable_style_->RemoveEquivalentProperties(style_at_position);

  DCHECK(editing_style_at_position->node_);
  if (IsRedundantTextAlign(mutable_style_.Get(), style_at_position,
                           editing_style_at_position->node_))
    mutable_style_->RemoveProperty(CSSPropertyID::kTextAlign);

  if (GetFontColor(mutable_style_.Get()) == GetFontColor(style_at_position))
    mutable_style_->RemoveProperty(CSSPropertyID::kColor);

  if (EditingStyleUtilities::HasTransparentBackgroundColor(
          mutable_style_.Get()) ||
      CssValueToColor(mutable_style_->GetPropertyCSSValue(
          CSSPropertyID::kBackgroundColor)) ==
          BackgroundColorInEffect(position.ComputeContainerNode()))
    mutable_style_->RemoveProperty(CSSPropertyID::kBackgroundColor);

  if (auto* unicode_bidi_identifier_value =
          DynamicTo<CSSIdentifierValue>(unicode_bidi)) {
    mutable_style_->SetLonghandProperty(
        CSSPropertyID::kUnicodeBidi,
        unicode_bidi_identifier_value->GetValueID());
    if (auto* direction_identifier_value =
            DynamicTo<CSSIdentifierValue>(direction)) {
      mutable_style_->SetLonghandProperty(
          CSSPropertyID::kDirection, direction_identifier_value->GetValueID());
    }
  }
}

void EditingStyle::MergeTypingStyle(Document* document) {
  DCHECK(document);

  EditingStyle* typing_style = document->GetFrame()->GetEditor().TypingStyle();
  if (!typing_style || typing_style == this)
    return;

  MergeStyle(typing_style->Style(), kOverrideValues);
}

void EditingStyle::MergeInlineStyleOfElement(
    HTMLElement* element,
    CSSPropertyOverrideMode mode,
    PropertiesToInclude properties_to_include) {
  DCHECK(element);
  if (!element->InlineStyle())
    return;

  switch (properties_to_include) {
    case kAllProperties:
      MergeStyle(element->InlineStyle(), mode);
      return;
    case kOnlyEditingInheritableProperties:
      MergeStyle(CopyEditingProperties(element->GetExecutionContext(),
                                       element->InlineStyle(),
                                       kOnlyInheritableEditingProperties),
                 mode);
      return;
    case kEditingPropertiesInEffect:
      MergeStyle(
          CopyEditingProperties(element->GetExecutionContext(),
                                element->InlineStyle(), kAllEditingProperties),
          mode);
      return;
  }
}

static inline bool ElementMatchesAndPropertyIsNotInInlineStyleDecl(
    const HTMLElementEquivalent* equivalent,
    const Element* element,
    EditingStyle::CSSPropertyOverrideMode mode,
    CSSPropertyValueSet* style) {
  return equivalent->Matches(element) &&
         (!element->InlineStyle() ||
          !equivalent->PropertyExistsInStyle(element->InlineStyle())) &&
         (mode == EditingStyle::kOverrideValues ||
          !equivalent->PropertyExistsInStyle(style));
}

static MutableCSSPropertyValueSet* ExtractEditingProperties(
    const ExecutionContext* execution_context,
    const CSSPropertyValueSet* style,
    EditingStyle::PropertiesToInclude properties_to_include) {
  if (!style)
    return nullptr;

  switch (properties_to_include) {
    case EditingStyle::kAllProperties:
    case EditingStyle::kEditingPropertiesInEffect:
      return CopyEditingProperties(execution_context, style,
                                   kAllEditingProperties);
    case EditingStyle::kOnlyEditingInheritableProperties:
      return CopyEditingProperties(execution_context, style,
                                   kOnlyInheritableEditingProperties);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void EditingStyle::MergeInlineAndImplicitStyleOfElement(
    Element* element,
    CSSPropertyOverrideMode mode,
    PropertiesToInclude properties_to_include) {
  EditingStyle* style_from_rules = MakeGarbageCollected<EditingStyle>();
  style_from_rules->MergeStyleFromRulesForSerialization(element);

  if (element->InlineStyle())
    style_from_rules->mutable_style_->MergeAndOverrideOnConflict(
        element->InlineStyle());

  style_from_rules->mutable_style_ = ExtractEditingProperties(
      element->GetExecutionContext(), style_from_rules->mutable_style_.Get(),
      properties_to_include);
  MergeStyle(style_from_rules->mutable_style_.Get(), mode);

  const HeapVector<Member<HTMLElementEquivalent>>& element_equivalents =
      HtmlElementEquivalents();
  for (const auto& equivalent : element_equivalents) {
    if (ElementMatchesAndPropertyIsNotInInlineStyleDecl(
            equivalent.Get(), element, mode, mutable_style_.Get()))
      equivalent->AddToStyle(element, this);
  }

  const HeapVector<Member<HTMLAttributeEquivalent>>& attribute_equivalents =
      HtmlAttributeEquivalents();
  for (const auto& attribute : attribute_equivalents) {
    if (attribute->AttributeName() == html_names::kDirAttr)
      continue;  // We don't want to include directionality
    if (ElementMatchesAndPropertyIsNotInInlineStyleDecl(
            attribute.Get(), element, mode, mutable_style_.Get()))
      attribute->AddToStyle(element, this);
  }
}

static const CSSValueList& MergeTextDecorationValues(
    const CSSValueList& merged_value,
    const CSSValueList& value_to_merge) {
  DEFINE_STATIC_LOCAL(Persistent<CSSIdentifierValue>, underline,
                      (CSSIdentifierValue::Create(CSSValueID::kUnderline)));
  DEFINE_STATIC_LOCAL(Persistent<CSSIdentifierValue>, line_through,
                      (CSSIdentifierValue::Create(CSSValueID::kLineThrough)));
  CSSValueList& result = *merged_value.Copy();
  if (value_to_merge.HasValue(*underline) && !merged_value.HasValue(*underline))
    result.Append(*underline);

  if (value_to_merge.HasValue(*line_through) &&
      !merged_value.HasValue(*line_through))
    result.Append(*line_through);

  return result;
}

void EditingStyle::MergeStyle(const CSSPropertyValueSet* style,
                              CSSPropertyOverrideMode mode) {
  if (!style)
    return;

  if (!mutable_style_) {
    mutable_style_ = style->MutableCopy();
    return;
  }

  unsigned property_count = style->PropertyCount();
  for (unsigned i = 0; i < property_count; ++i) {
    CSSPropertyValueSet::PropertyReference property = style->PropertyAt(i);
    const CSSValue* value = mutable_style_->GetPropertyCSSValue(property.Id());

    // text decorations never override values
    const auto* property_value_list = DynamicTo<CSSValueList>(property.Value());
    if ((property.Id() == CSSPropertyID::kTextDecorationLine ||
         property.Id() == CSSPropertyID::kWebkitTextDecorationsInEffect) &&
        property_value_list && value) {
      if (const auto* value_list = DynamicTo<CSSValueList>(value)) {
        const CSSValueList& result =
            MergeTextDecorationValues(*value_list, *property_value_list);
        mutable_style_->SetProperty(property.Id(), result,
                                    property.IsImportant());
        continue;
      }
      // text-decoration: none is equivalent to not having the property
      value = nullptr;
    }

    if (mode == kOverrideValues || (mode == kDoNotOverrideValues && !value)) {
      mutable_style_->SetLonghandProperty(
          CSSPropertyValue(property.PropertyMetadata(), property.Value()));
    }
  }
}

static MutableCSSPropertyValueSet* StyleFromMatchedRulesForElement(
    Element* element,
    unsigned rules_to_include) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  StyleRuleList* matched_rules =
      element->GetDocument().GetStyleResolver().StyleRulesForElement(
          element, rules_to_include);
  if (matched_rules) {
    for (unsigned i = 0; i < matched_rules->size(); ++i)
      style->MergeAndOverrideOnConflict(&matched_rules->at(i)->Properties());
  }
  return style;
}

void EditingStyle::MergeStyleFromRules(Element* element) {
  MutableCSSPropertyValueSet* style_from_matched_rules =
      StyleFromMatchedRulesForElement(element, StyleResolver::kAuthorCSSRules);
  // Styles from the inline style declaration, held in the variable "style",
  // take precedence over those from matched rules.
  if (mutable_style_)
    style_from_matched_rules->MergeAndOverrideOnConflict(mutable_style_.Get());

  Clear();
  mutable_style_ = style_from_matched_rules;
}

void EditingStyle::MergeStyleFromRulesForSerialization(Element* element) {
  MergeStyleFromRules(element);

  // The property value, if it's a percentage, may not reflect the actual
  // computed value.
  // For example: style="height: 1%; overflow: visible;" in quirksmode
  // FIXME: There are others like this, see <rdar://problem/5195123> Slashdot
  // copy/paste fidelity problem
  auto* computed_style_for_element =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element);
  auto* from_computed_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  {
    unsigned property_count = mutable_style_->PropertyCount();
    for (unsigned i = 0; i < property_count; ++i) {
      CSSPropertyValueSet::PropertyReference property =
          mutable_style_->PropertyAt(i);
      const CSSValue& value = property.Value();
      const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
      if (!primitive_value)
        continue;
      if (primitive_value->IsPercentage()) {
        CSSPropertyName name = property.Name();
        if (const CSSValue* computed_property_value =
                computed_style_for_element->GetPropertyCSSValue(name)) {
          from_computed_style->AddRespectingCascade(
              CSSPropertyValue(name, *computed_property_value));
        }
      }
    }
  }
  mutable_style_->MergeAndOverrideOnConflict(from_computed_style);

 // There are some scenarios, like when copying rich text while in ForcedColors
  // mode where we don't want to keep the ForcedColors styling, so that if it is
  // pasted and sent to someone with no ForcedColors applied it does not affect
  // their styling.
  if (element->GetDocument().InForcedColorsMode()) {
    mutable_style_->SetLonghandProperty(CSSPropertyID::kBackgroundColor,
                                        CSSValueID::kInitial, false);
    mutable_style_->SetLonghandProperty(CSSPropertyID::kColor,
                                        CSSValueID::kInitial, false);
    mutable_style_->SetLonghandProperty(CSSPropertyID::kTextDecorationColor,
                                        CSSValueID::kInitial, false);
  }
}

static void RemovePropertiesInStyle(
    MutableCSSPropertyValueSet* style_to_remove_properties_from,
    CSSPropertyValueSet* style) {
  unsigned property_count = style->PropertyCount();
  Vector<const CSSProperty*> properties_to_remove(property_count);
  for (unsigned i = 0; i < property_count; ++i) {
    // TODO(crbug.com/980160): Remove access to static Variable instance.
    properties_to_remove[i] = &CSSProperty::Get(style->PropertyAt(i).Id());
  }

  style_to_remove_properties_from->RemovePropertiesInSet(properties_to_remove);
}

void EditingStyle::RemoveStyleFromRulesAndContext(Element* element,
                                                  Element* context) {
  DCHECK(element);
  if (!mutable_style_)
    return;

  // StyleResolver requires clean style.
  DCHECK_GE(element->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kStyleClean);
  DCHECK(element->GetDocument().IsActive());

  SecureContextMode secure_context_mode =
      element->GetExecutionContext()->GetSecureContextMode();

  // 1. Remove style from matched rules because style remain without repeating
  // it in inline style declaration
  MutableCSSPropertyValueSet* style_from_matched_rules =
      StyleFromMatchedRulesForElement(element, StyleResolver::kAllCSSRules);
  if (style_from_matched_rules && !style_from_matched_rules->IsEmpty()) {
    mutable_style_ =
        GetPropertiesNotIn(mutable_style_.Get(), element,
                           style_from_matched_rules->EnsureCSSStyleDeclaration(
                               element->GetExecutionContext()),
                           secure_context_mode);
  }

  // 2. Remove style present in context and not overriden by matched rules.
  EditingStyle* computed_style =
      MakeGarbageCollected<EditingStyle>(context, kEditingPropertiesInEffect);
  if (computed_style->mutable_style_) {
    if (!computed_style->mutable_style_->GetPropertyCSSValue(
            CSSPropertyID::kBackgroundColor)) {
      computed_style->mutable_style_->SetLonghandProperty(
          CSSPropertyID::kBackgroundColor, CSSValueID::kTransparent);
    }

    RemovePropertiesInStyle(computed_style->mutable_style_.Get(),
                            style_from_matched_rules);
    mutable_style_ = GetPropertiesNotIn(
        mutable_style_.Get(), element,
        computed_style->mutable_style_->EnsureCSSStyleDeclaration(
            element->GetExecutionContext()),
        secure_context_mode);
  }

  // 3. If this element is a span and has display: inline or float: none, remove
  // them unless they are overriden by rules. These rules are added by
  // serialization code to wrap text nodes.
  if (IsStyleSpanOrSpanWithOnlyStyleAttribute(element)) {
    if (!style_from_matched_rules->GetPropertyCSSValue(
            CSSPropertyID::kDisplay) &&
        GetProperty(CSSPropertyID::kDisplay) == CSSValueID::kInline)
      mutable_style_->RemoveProperty(CSSPropertyID::kDisplay);
    if (!style_from_matched_rules->GetPropertyCSSValue(CSSPropertyID::kFloat) &&
        GetProperty(CSSPropertyID::kFloat) == CSSValueID::kNone)
      mutable_style_->RemoveProperty(CSSPropertyID::kFloat);
  }
}

void EditingStyle::RemovePropertiesInElementDefaultStyle(Element* element) {
  if (!mutable_style_ || mutable_style_->IsEmpty())
    return;

  CSSPropertyValueSet* default_style = StyleFromMatchedRulesForElement(
      element, StyleResolver::kUAAndUserCSSRules);

  RemovePropertiesInStyle(mutable_style_.Get(), default_style);
}

void EditingStyle::ForceInline() {
  if (!mutable_style_) {
    mutable_style_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  }
  const bool kPropertyIsImportant = true;
  mutable_style_->SetLonghandProperty(
      CSSPropertyID::kDisplay, CSSValueID::kInline, kPropertyIsImportant);
}

int EditingStyle::LegacyFontSize(Document* document) const {
  const CSSValue* css_value =
      mutable_style_->GetPropertyCSSValue(CSSPropertyID::kFontSize);
  if (!css_value ||
      !(css_value->IsPrimitiveValue() || css_value->IsIdentifierValue()))
    return 0;
  return LegacyFontSizeFromCSSValue(document, css_value, is_monospace_font_,
                                    kAlwaysUseLegacyFontSize);
}

void EditingStyle::Trace(Visitor* visitor) const {
  visitor->Trace(mutable_style_);
  visitor->Trace(node_);
}

static void ReconcileTextDecorationProperties(
    MutableCSSPropertyValueSet* style,
    SecureContextMode secure_context_mode) {
  const CSSValue* text_decorations_in_effect =
      style->GetPropertyCSSValue(CSSPropertyID::kWebkitTextDecorationsInEffect);
  const CSSValue* text_decoration =
      style->GetPropertyCSSValue(CSSPropertyID::kTextDecorationLine);
  // "web_tests/editing/execCommand/insert-list-and-strikethrough.html" makes
  // both |textDecorationsInEffect| and |textDecoration| non-null.
  if (text_decorations_in_effect) {
    style->ParseAndSetProperty(CSSPropertyID::kTextDecorationLine,
                               text_decorations_in_effect->CssText(),
                               /* important */ false, secure_context_mode);
    style->RemoveProperty(CSSPropertyID::kWebkitTextDecorationsInEffect);
    text_decoration = text_decorations_in_effect;
  }

  // If text-decoration is set to "none", remove the property because we don't
  // want to add redundant "text-decoration: none".
  if (text_decoration && !text_decoration->IsValueList())
    style->RemoveProperty(CSSPropertyID::kTextDecorationLine);
}

StyleChange::StyleChange(EditingStyle* style, const Position& position)
    : apply_bold_(false),
      apply_italic_(false),
      apply_underline_(false),
      apply_line_through_(false),
      apply_subscript_(false),
      apply_superscript_(false) {
  Document* document = position.GetDocument();
  if (!style || !style->Style() || !document || !document->GetFrame())
    return;
  Element* const element = AssociatedElementOf(position);
  if (!element)
    return;

  CSSComputedStyleDeclaration* const computed_style =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(element);
  // FIXME: take care of background-color in effect
  // Note: editing/undo/redo-selection-modify-crash.html needs to pass
  // |element| to |GetPropertiesNotIn()| to remove "text-align:left".
  MutableCSSPropertyValueSet* mutable_style = GetPropertiesNotIn(
      style->Style(), element, computed_style,
      document->GetExecutionContext()->GetSecureContextMode());
  DCHECK(mutable_style);

  ReconcileTextDecorationProperties(
      mutable_style, document->GetExecutionContext()->GetSecureContextMode());
  if (!document->GetFrame()->GetEditor().ShouldStyleWithCSS())
    ExtractTextStyles(document, mutable_style,
                      computed_style->IsMonospaceFont());

  // If unicode-bidi is present in mutableStyle and direction is not, then add
  // direction to mutableStyle.
  // FIXME: Shouldn't this be done in getPropertiesNotIn?
  if (mutable_style->GetPropertyCSSValue(CSSPropertyID::kUnicodeBidi) &&
      !style->Style()->GetPropertyCSSValue(CSSPropertyID::kDirection)) {
    mutable_style->ParseAndSetProperty(
        CSSPropertyID::kDirection,
        style->Style()->GetPropertyValue(CSSPropertyID::kDirection),
        /* important */ false,
        document->GetExecutionContext()->GetSecureContextMode());
  }

  // Save the result for later
  css_style_ = mutable_style->AsText().StripWhiteSpace();
}

static void SetTextDecorationProperty(MutableCSSPropertyValueSet* style,
                                      const CSSValueList* new_text_decoration,
                                      CSSPropertyID property_id,
                                      SecureContextMode secure_context_mode) {
  if (new_text_decoration->length()) {
    style->ParseAndSetProperty(property_id, new_text_decoration->CssText(),
                               style->PropertyIsImportant(property_id),
                               secure_context_mode);
  } else {
    // text-decoration: none is redundant since it does not remove any text
    // decorations.
    style->RemoveProperty(property_id);
  }
}

static bool GetPrimitiveValueNumber(CSSPropertyValueSet* style,
                                    CSSPropertyID property_id,
                                    float& number) {
  if (!style)
    return false;
  const CSSValue* value = style->GetPropertyCSSValue(property_id);
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return false;
  number = primitive_value->GetFloatValue();
  return true;
}

void StyleChange::ExtractTextStyles(Document* document,
                                    MutableCSSPropertyValueSet* style,
                                    bool is_monospace_font) {
  DCHECK(style);

  float weight = 0;
  bool is_number =
      GetPrimitiveValueNumber(style, CSSPropertyID::kFontWeight, weight);
  if (GetIdentifierValue(style, CSSPropertyID::kFontWeight) ==
          CSSValueID::kBold ||
      (is_number && weight >= kBoldThreshold)) {
    style->RemoveProperty(CSSPropertyID::kFontWeight);
    apply_bold_ = true;
  }

  CSSValueID font_style = GetIdentifierValue(style, CSSPropertyID::kFontStyle);
  if (font_style == CSSValueID::kItalic || font_style == CSSValueID::kOblique) {
    style->RemoveProperty(CSSPropertyID::kFontStyle);
    apply_italic_ = true;
  }

  // Assuming reconcileTextDecorationProperties has been called, there should
  // not be -webkit-text-decorations-in-effect
  // Furthermore, text-decoration: none has been trimmed so that text-decoration
  // property is always a CSSValueList.
  const CSSValue* text_decoration =
      style->GetPropertyCSSValue(CSSPropertyID::kTextDecorationLine);
  if (const auto* text_decoration_value_list =
          DynamicTo<CSSValueList>(text_decoration)) {
    DEFINE_STATIC_LOCAL(Persistent<CSSIdentifierValue>, underline,
                        (CSSIdentifierValue::Create(CSSValueID::kUnderline)));
    DEFINE_STATIC_LOCAL(Persistent<CSSIdentifierValue>, line_through,
                        (CSSIdentifierValue::Create(CSSValueID::kLineThrough)));
    CSSValueList* new_text_decoration = text_decoration_value_list->Copy();
    if (new_text_decoration->RemoveAll(*underline))
      apply_underline_ = true;
    if (new_text_decoration->RemoveAll(*line_through))
      apply_line_through_ = true;

    // If trimTextDecorations, delete underline and line-through
    SetTextDecorationProperty(
        style, new_text_decoration, CSSPropertyID::kTextDecorationLine,
        document->GetExecutionContext()->GetSecureContextMode());
  }

  CSSValueID vertical_align =
      GetIdentifierValue(style, CSSPropertyID::kVerticalAlign);
  switch (vertical_align) {
    case CSSValueID::kSub:
      style->RemoveProperty(CSSPropertyID::kVerticalAlign);
      apply_subscript_ = true;
      break;
    case CSSValueID::kSuper:
      style->RemoveProperty(CSSPropertyID::kVerticalAlign);
      apply_superscript_ = true;
      break;
    default:
      break;
  }

  if (style->GetPropertyCSSValue(CSSPropertyID::kColor)) {
    // The <font> tag cannot handle rgb colors, so we need to serialize as hex
    // here in order to continue supporting it.
    apply_font_color_ = GetFontColor(style).SerializeAsCanvasColor();
    style->RemoveProperty(CSSPropertyID::kColor);
  }

  apply_font_face_ = style->GetPropertyValue(CSSPropertyID::kFontFamily);
  // Remove double quotes for Outlook 2007 compatibility. See
  // https://bugs.webkit.org/show_bug.cgi?id=79448
  apply_font_face_.Replace('"', "");
  style->RemoveProperty(CSSPropertyID::kFontFamily);

  if (const CSSValue* font_size =
          style->GetPropertyCSSValue(CSSPropertyID::kFontSize)) {
    if (!font_size->IsPrimitiveValue() && !font_size->IsIdentifierValue()) {
      // Can't make sense of the number. Put no font size.
      style->RemoveProperty(CSSPropertyID::kFontSize);
    } else if (int legacy_font_size = LegacyFontSizeFromCSSValue(
                   document, font_size, is_monospace_font,
                   kUseLegacyFontSizeOnlyIfPixelValuesMatch)) {
      apply_font_size_ = String::Number(legacy_font_size);
      style->RemoveProperty(CSSPropertyID::kFontSize);
    }
  }
}

static void DiffTextDecorations(MutableCSSPropertyValueSet* style,
                                CSSPropertyID property_id,
                                const CSSValue* ref_text_decoration,
                                SecureContextMode secure_context_mode) {
  const CSSValue* text_decoration = style->GetPropertyCSSValue(property_id);
  const auto* values_in_text_decoration =
      DynamicTo<CSSValueList>(text_decoration);
  const auto* values_in_ref_text_decoration =
      DynamicTo<CSSValueList>(ref_text_decoration);
  if (!values_in_text_decoration || !values_in_ref_text_decoration)
    return;

  CSSValueList* new_text_decoration = values_in_text_decoration->Copy();

  for (wtf_size_t i = 0; i < values_in_ref_text_decoration->length(); i++)
    new_text_decoration->RemoveAll(values_in_ref_text_decoration->Item(i));

  SetTextDecorationProperty(style, new_text_decoration, property_id,
                            secure_context_mode);
}

static bool FontWeightIsBold(const CSSValue* font_weight) {
  if (auto* font_weight_identifier_value =
          DynamicTo<CSSIdentifierValue>(font_weight)) {
    // Because b tag can only bold text, there are only two states in plain
    // html: bold and not bold. Collapse all other values to either one of these
    // two states for editing purposes.

    switch (font_weight_identifier_value->GetValueID()) {
      case CSSValueID::kNormal:
        return false;
      case CSSValueID::kBold:
        return true;
      default:
        break;
    }
  }

  CHECK(To<CSSPrimitiveValue>(font_weight)->IsNumber());
  return To<CSSPrimitiveValue>(font_weight)->GetFloatValue() >= kBoldThreshold;
}

static bool FontWeightNeedsResolving(const CSSValue* font_weight) {
  if (font_weight->IsPrimitiveValue())
    return false;
  auto* font_weight_identifier_value =
      DynamicTo<CSSIdentifierValue>(font_weight);
  if (!font_weight_identifier_value)
    return true;
  const CSSValueID value = font_weight_identifier_value->GetValueID();
  return value == CSSValueID::kLighter || value == CSSValueID::kBolder;
}

MutableCSSPropertyValueSet* GetPropertiesNotIn(
    CSSPropertyValueSet* style_with_redundant_properties,
    Node* node,
    CSSStyleDeclaration* base_style,
    SecureContextMode secure_context_mode) {
  DCHECK(style_with_redundant_properties);
  DCHECK(node);
  DCHECK(base_style);
  MutableCSSPropertyValueSet* result =
      style_with_redundant_properties->MutableCopy();

  result->RemoveEquivalentProperties(base_style);

  const CSSValue* base_text_decorations_in_effect =
      base_style->GetPropertyCSSValueInternal(
          CSSPropertyID::kWebkitTextDecorationsInEffect);
  DiffTextDecorations(result, CSSPropertyID::kTextDecorationLine,
                      base_text_decorations_in_effect, secure_context_mode);
  DiffTextDecorations(result, CSSPropertyID::kWebkitTextDecorationsInEffect,
                      base_text_decorations_in_effect, secure_context_mode);

  if (const CSSValue* base_font_weight =
          base_style->GetPropertyCSSValueInternal(CSSPropertyID::kFontWeight)) {
    if (const CSSValue* font_weight =
            result->GetPropertyCSSValue(CSSPropertyID::kFontWeight)) {
      if (!FontWeightNeedsResolving(font_weight) &&
          !FontWeightNeedsResolving(base_font_weight) &&
          (FontWeightIsBold(font_weight) == FontWeightIsBold(base_font_weight)))
        result->RemoveProperty(CSSPropertyID::kFontWeight);
    }
  }

  if (base_style->GetPropertyCSSValueInternal(CSSPropertyID::kColor) &&
      GetFontColor(result) == GetFontColor(base_style))
    result->RemoveProperty(CSSPropertyID::kColor);

  if (IsRedundantTextAlign(result, base_style, node))
    result->RemoveProperty(CSSPropertyID::kTextAlign);

  if (base_style->GetPropertyCSSValueInternal(
          CSSPropertyID::kBackgroundColor) &&
      GetBackgroundColor(result) == GetBackgroundColor(base_style))
    result->RemoveProperty(CSSPropertyID::kBackgroundColor);

  return result;
}

CSSValueID GetIdentifierValue(CSSPropertyValueSet* style,
                              CSSPropertyID property_id) {
  if (!style)
    return CSSValueID::kInvalid;
  const CSSValue* value = style->GetPropertyCSSValue(property_id);
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return CSSValueID::kInvalid;
  return identifier_value->GetValueID();
}

CSSValueID GetIdentifierValue(CSSStyleDeclaration* style,
                              CSSPropertyID property_id) {
  if (!style)
    return CSSValueID::kInvalid;
  const CSSValue* value = style->GetPropertyCSSValueInternal(property_id);
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return CSSValueID::kInvalid;
  return identifier_value->GetValueID();
}

int LegacyFontSizeFromCSSValue(Document* document,
                               const CSSValue* value,
                               bool is_monospace_font,
                               LegacyFontSizeMode mode) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsLength()) {
      // TODO(crbug.com/979895): This doesn't seem to be handle math functions
      // correctly. This is the result of a refactoring, and may have revealed
      // an existing bug. Fix it if necessary.
      CSSPrimitiveValue::UnitType length_unit =
          primitive_value->IsNumericLiteralValue()
              ? To<CSSNumericLiteralValue>(primitive_value)->GetType()
              : CSSPrimitiveValue::UnitType::kPixels;
      if (!CSSPrimitiveValue::IsRelativeUnit(length_unit)) {
        double conversion =
            CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                length_unit);
        int pixel_font_size =
            ClampTo<int>(primitive_value->GetDoubleValue() * conversion);
        int legacy_font_size = FontSizeFunctions::LegacyFontSize(
            document, pixel_font_size, is_monospace_font);
        // Use legacy font size only if pixel value matches exactly to that of
        // legacy font size.
        if (mode == kAlwaysUseLegacyFontSize ||
            FontSizeFunctions::FontSizeForKeyword(document, legacy_font_size,
                                                  is_monospace_font) ==
                pixel_font_size)
          return legacy_font_size;

        return 0;
      }
    }
  }

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    if (identifier_value->GetValueID() == CSSValueID::kWebkitXxxLarge)
      return FontSizeFunctions::KeywordSize(CSSValueID::kXxxLarge) - 1;
    if (CSSValueID::kXSmall <= identifier_value->GetValueID() &&
        identifier_value->GetValueID() <= CSSValueID::kXxxLarge)
      return FontSizeFunctions::KeywordSize(identifier_value->GetValueID()) - 1;
  }

  return 0;
}

EditingTriState EditingStyle::SelectionHasStyle(const LocalFrame& frame,
                                                CSSPropertyID property_id,
                                                const String& value) {
  const SecureContextMode secure_context_mode =
      frame.DomWindow()->GetSecureContextMode();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  return MakeGarbageCollected<EditingStyle>(property_id, value,
                                            secure_context_mode)
      ->TriStateOfStyle(frame.Selection().ComputeVisibleSelectionInDOMTree(),
                        secure_context_mode);
}

}  // namespace blink
