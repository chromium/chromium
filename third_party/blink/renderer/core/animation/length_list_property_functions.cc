// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/length_list_property_functions.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

const FillLayer* GetFillLayerForPosition(const CSSProperty& property,
                                         const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
      return &style.BackgroundLayers();
    case CSSPropertyID::kWebkitMaskPositionX:
    case CSSPropertyID::kWebkitMaskPositionY:
      return &style.MaskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

FillLayer* AccessFillLayerForPosition(const CSSProperty& property,
                                      ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
      return &style.AccessBackgroundLayers();
    case CSSPropertyID::kWebkitMaskPositionX:
    case CSSPropertyID::kWebkitMaskPositionY:
      return &style.AccessMaskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

struct FillLayerMethods {
  FillLayerMethods(const CSSProperty& property) {
    switch (property.PropertyID()) {
      case CSSPropertyID::kBackgroundPositionX:
      case CSSPropertyID::kWebkitMaskPositionX:
        is_set = &FillLayer::IsPositionXSet;
        get_length = &FillLayer::PositionX;
        get_edge = &FillLayer::BackgroundXOrigin;
        set_length = &FillLayer::SetPositionX;
        clear = &FillLayer::ClearPositionX;
        break;
      case CSSPropertyID::kBackgroundPositionY:
      case CSSPropertyID::kWebkitMaskPositionY:
        is_set = &FillLayer::IsPositionYSet;
        get_length = &FillLayer::PositionY;
        get_edge = &FillLayer::BackgroundYOrigin;
        set_length = &FillLayer::SetPositionY;
        clear = &FillLayer::ClearPositionY;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  bool (FillLayer::*is_set)() const = nullptr;
  const Length& (FillLayer::*get_length)() const = nullptr;
  BackgroundEdgeOrigin (FillLayer::*get_edge)() const = nullptr;
  void (FillLayer::*set_length)(const Length&) = nullptr;
  void (FillLayer::*clear)() = nullptr;
};

}  // namespace

ValueRange LengthListPropertyFunctions::GetValueRange(
    const CSSProperty& property) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kObjectPosition:
    case CSSPropertyID::kOffsetAnchor:
    case CSSPropertyID::kOffsetPosition:
    case CSSPropertyID::kPerspectiveOrigin:
    case CSSPropertyID::kTransformOrigin:
    case CSSPropertyID::kWebkitMaskPositionX:
    case CSSPropertyID::kWebkitMaskPositionY:
      return kValueRangeAll;

    case CSSPropertyID::kBorderBottomLeftRadius:
    case CSSPropertyID::kBorderBottomRightRadius:
    case CSSPropertyID::kBorderTopLeftRadius:
    case CSSPropertyID::kBorderTopRightRadius:
    case CSSPropertyID::kStrokeDasharray:
      return kValueRangeNonNegative;

    default:
      NOTREACHED();
      return kValueRangeAll;
  }
}

bool LengthListPropertyFunctions::GetInitialLengthList(
    const CSSProperty& property,
    Vector<Length>& result) {
  return GetLengthList(property, ComputedStyle::InitialStyle(), result);
}

static bool AppendToVector(const LengthPoint& point, Vector<Length>& result) {
  result.push_back(point.X());
  result.push_back(point.Y());
  return true;
}

static bool AppendToVector(const LengthSize& size, Vector<Length>& result) {
  result.push_back(size.Width());
  result.push_back(size.Height());
  return true;
}

static bool AppendToVector(const TransformOrigin& transform_origin,
                           Vector<Length>& result) {
  result.push_back(transform_origin.X());
  result.push_back(transform_origin.Y());
  result.push_back(Length::Fixed(transform_origin.Z()));
  return true;
}

bool LengthListPropertyFunctions::GetLengthList(const CSSProperty& property,
                                                const ComputedStyle& style,
                                                Vector<Length>& result) {
  DCHECK(result.IsEmpty());

  switch (property.PropertyID()) {
    case CSSPropertyID::kStrokeDasharray: {
      if (style.StrokeDashArray())
        result.AppendVector(style.StrokeDashArray()->data);
      return true;
    }

    case CSSPropertyID::kObjectPosition:
      return AppendToVector(style.ObjectPosition(), result);
    case CSSPropertyID::kOffsetAnchor:
      return AppendToVector(style.OffsetAnchor(), result);
    case CSSPropertyID::kOffsetPosition:
      return AppendToVector(style.OffsetPosition(), result);
    case CSSPropertyID::kPerspectiveOrigin:
      return AppendToVector(style.PerspectiveOrigin(), result);
    case CSSPropertyID::kBorderBottomLeftRadius:
      return AppendToVector(style.BorderBottomLeftRadius(), result);
    case CSSPropertyID::kBorderBottomRightRadius:
      return AppendToVector(style.BorderBottomRightRadius(), result);
    case CSSPropertyID::kBorderTopLeftRadius:
      return AppendToVector(style.BorderTopLeftRadius(), result);
    case CSSPropertyID::kBorderTopRightRadius:
      return AppendToVector(style.BorderTopRightRadius(), result);
    case CSSPropertyID::kTransformOrigin:
      return AppendToVector(style.GetTransformOrigin(), result);

    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kWebkitMaskPositionX:
    case CSSPropertyID::kWebkitMaskPositionY: {
      const FillLayer* fill_layer = GetFillLayerForPosition(property, style);
      FillLayerMethods fill_layer_methods(property);
      while (fill_layer && (fill_layer->*fill_layer_methods.is_set)()) {
        result.push_back((fill_layer->*fill_layer_methods.get_length)());
        switch ((fill_layer->*fill_layer_methods.get_edge)()) {
          case BackgroundEdgeOrigin::kRight:
          case BackgroundEdgeOrigin::kBottom:
            result.back() = result.back().SubtractFromOneHundredPercent();
            break;
          default:
            break;
        }
        fill_layer = fill_layer->Next();
      }
      return true;
    }

    default:
      NOTREACHED();
      return false;
  }
}

static LengthPoint PointFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 2U);
  return LengthPoint(list[0], list[1]);
}

static LengthSize SizeFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 2U);
  return LengthSize(list[0], list[1]);
}

static TransformOrigin TransformOriginFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 3U);
  return TransformOrigin(list[0], list[1], list[2].Pixels());
}

void LengthListPropertyFunctions::SetLengthList(const CSSProperty& property,
                                                ComputedStyle& style,
                                                Vector<Length>&& length_list) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kStrokeDasharray:
      style.SetStrokeDashArray(
          length_list.IsEmpty()
              ? nullptr
              : base::MakeRefCounted<SVGDashArray>(std::move(length_list)));
      return;

    case CSSPropertyID::kObjectPosition:
      style.SetObjectPosition(PointFromVector(length_list));
      return;
    case CSSPropertyID::kOffsetAnchor:
      style.SetOffsetAnchor(PointFromVector(length_list));
      return;
    case CSSPropertyID::kOffsetPosition:
      style.SetOffsetPosition(PointFromVector(length_list));
      return;
    case CSSPropertyID::kPerspectiveOrigin:
      style.SetPerspectiveOrigin(PointFromVector(length_list));
      return;

    case CSSPropertyID::kBorderBottomLeftRadius:
      style.SetBorderBottomLeftRadius(SizeFromVector(length_list));
      return;
    case CSSPropertyID::kBorderBottomRightRadius:
      style.SetBorderBottomRightRadius(SizeFromVector(length_list));
      return;
    case CSSPropertyID::kBorderTopLeftRadius:
      style.SetBorderTopLeftRadius(SizeFromVector(length_list));
      return;
    case CSSPropertyID::kBorderTopRightRadius:
      style.SetBorderTopRightRadius(SizeFromVector(length_list));
      return;

    case CSSPropertyID::kTransformOrigin:
      style.SetTransformOrigin(TransformOriginFromVector(length_list));
      return;

    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kWebkitMaskPositionX:
    case CSSPropertyID::kWebkitMaskPositionY: {
      FillLayer* fill_layer = AccessFillLayerForPosition(property, style);
      FillLayer* prev = nullptr;
      FillLayerMethods fill_layer_methods(property);
      for (wtf_size_t i = 0; i < length_list.size(); i++) {
        if (!fill_layer)
          fill_layer = prev->EnsureNext();
        (fill_layer->*fill_layer_methods.set_length)(length_list[i]);
        prev = fill_layer;
        fill_layer = fill_layer->Next();
      }
      while (fill_layer) {
        (fill_layer->*fill_layer_methods.clear)();
        fill_layer = fill_layer->Next();
      }
      return;
    }

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace blink
