// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name)
    : CSSImageGeneratorValue(kPaintClass),
      name_(name),
      paint_image_generator_observer_(new Observer(this)) {}

CSSPaintValue::CSSPaintValue(
    CSSCustomIdentValue* name,
    Vector<scoped_refptr<CSSVariableData>>& variable_data)
    : CSSPaintValue(name) {
  argument_variable_data_.swap(variable_data);
}

CSSPaintValue::~CSSPaintValue() = default;

String CSSPaintValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("paint(");
  result.Append(name_->CustomCSSText());
  for (const auto& variable_data : argument_variable_data_) {
    result.Append(", ");
    result.Append(variable_data.get()->TokenRange().Serialize());
  }
  result.Append(')');
  return result.ToString();
}

String CSSPaintValue::GetName() const {
  return name_->Value();
}

scoped_refptr<Image> CSSPaintValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const FloatSize& target_size) {
  // https://crbug.com/835589: early exit when paint target is associated with
  // a link.
  if (style.InsideLink() != EInsideLink::kNotInsideLink)
    return nullptr;

  if (!generator_) {
    generator_ = CSSPaintImageGenerator::Create(
        GetName(), document, paint_image_generator_observer_);
  }

  if (!ParseInputArguments(document))
    return nullptr;

  return generator_->Paint(client, target_size, parsed_input_arguments_);
}

bool CSSPaintValue::ParseInputArguments(const Document& document) {
  if (input_arguments_invalid_)
    return false;

  if (parsed_input_arguments_ ||
      !RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled())
    return true;

  if (!generator_->IsImageGeneratorReady())
    return false;

  const Vector<CSSSyntaxDescriptor>& input_argument_types =
      generator_->InputArgumentTypes();
  if (argument_variable_data_.size() != input_argument_types.size()) {
    input_arguments_invalid_ = true;
    return false;
  }

  parsed_input_arguments_ = new CSSStyleValueVector();

  for (wtf_size_t i = 0; i < argument_variable_data_.size(); ++i) {
    // If we are parsing a paint() function, we must be a secure context.
    DCHECK_EQ(SecureContextMode::kSecureContext,
              document.GetSecureContextMode());
    const CSSValue* parsed_value = argument_variable_data_[i]->ParseForSyntax(
        input_argument_types[i], SecureContextMode::kSecureContext);
    if (!parsed_value) {
      input_arguments_invalid_ = true;
      parsed_input_arguments_ = nullptr;
      return false;
    }
    parsed_input_arguments_->AppendVector(
        StyleValueFactory::CssValueToStyleValueVector(*parsed_value));
  }
  return true;
}

void CSSPaintValue::Observer::PaintImageGeneratorReady() {
  owner_value_->PaintImageGeneratorReady();
}

void CSSPaintValue::PaintImageGeneratorReady() {
  for (const ImageResourceObserver* client : Clients().Keys()) {
    // TODO(ikilpatrick): We shouldn't be casting like this or mutate the layout
    // tree from a const pointer.
    const_cast<ImageResourceObserver*>(client)->ImageChanged(
        static_cast<WrappedImagePtr>(this),
        ImageResourceObserver::CanDeferInvalidation::kNo);
  }
}

bool CSSPaintValue::KnownToBeOpaque(const Document&,
                                    const ComputedStyle&) const {
  return generator_ && !generator_->HasAlpha();
}

bool CSSPaintValue::Equals(const CSSPaintValue& other) const {
  return GetName() == other.GetName() &&
         CustomCSSText() == other.CustomCSSText();
}

void CSSPaintValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(name_);
  visitor->Trace(generator_);
  visitor->Trace(paint_image_generator_observer_);
  visitor->Trace(parsed_input_arguments_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
