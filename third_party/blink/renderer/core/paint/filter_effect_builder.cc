/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_box_reflect.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_color_matrix.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_component_transfer.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_drop_shadow.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/graphics/interpolation_space.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {

inline void EndMatrixRow(Vector<float>& matrix) {
  matrix.UncheckedAppend(0);
  matrix.UncheckedAppend(0);
}

inline void LastMatrixRow(Vector<float>& matrix) {
  matrix.UncheckedAppend(0);
  matrix.UncheckedAppend(0);
  matrix.UncheckedAppend(0);
  matrix.UncheckedAppend(1);
  matrix.UncheckedAppend(0);
}

Vector<float> GrayscaleMatrix(double amount) {
  double one_minus_amount = ClampTo(1 - amount, 0.0, 1.0);

  // See https://drafts.fxtf.org/filter-effects/#grayscaleEquivalent for
  // information on parameters.
  Vector<float> matrix;
  matrix.ReserveInitialCapacity(20);

  matrix.UncheckedAppend(ClampTo<float>(0.2126 + 0.7874 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.7152 - 0.7152 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.0722 - 0.0722 * one_minus_amount));
  EndMatrixRow(matrix);

  matrix.UncheckedAppend(ClampTo<float>(0.2126 - 0.2126 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.7152 + 0.2848 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.0722 - 0.0722 * one_minus_amount));
  EndMatrixRow(matrix);

  matrix.UncheckedAppend(ClampTo<float>(0.2126 - 0.2126 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.7152 - 0.7152 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.0722 + 0.9278 * one_minus_amount));
  EndMatrixRow(matrix);

  LastMatrixRow(matrix);
  return matrix;
}

Vector<float> SepiaMatrix(double amount) {
  double one_minus_amount = ClampTo(1 - amount, 0.0, 1.0);

  // See https://drafts.fxtf.org/filter-effects/#sepiaEquivalent for information
  // on parameters.
  Vector<float> matrix;
  matrix.ReserveInitialCapacity(20);

  matrix.UncheckedAppend(ClampTo<float>(0.393 + 0.607 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.769 - 0.769 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.189 - 0.189 * one_minus_amount));
  EndMatrixRow(matrix);

  matrix.UncheckedAppend(ClampTo<float>(0.349 - 0.349 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.686 + 0.314 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.168 - 0.168 * one_minus_amount));
  EndMatrixRow(matrix);

  matrix.UncheckedAppend(ClampTo<float>(0.272 - 0.272 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.534 - 0.534 * one_minus_amount));
  matrix.UncheckedAppend(ClampTo<float>(0.131 + 0.869 * one_minus_amount));
  EndMatrixRow(matrix);

  LastMatrixRow(matrix);
  return matrix;
}

}  // namespace

FilterEffectBuilder::FilterEffectBuilder(const gfx::RectF& reference_box,
                                         std::optional<gfx::SizeF> viewport,
                                         float zoom,
                                         Color current_color,
                                         mojom::blink::ColorScheme color_scheme,
                                         const cc::PaintFlags* fill_flags,
                                         const cc::PaintFlags* stroke_flags)
    : reference_box_(reference_box),
      viewport_(
          RuntimeEnabledFeatures::SvgFilterUserSpaceViewportForNonSvgEnabled()
              ? viewport
              : std::nullopt),
      zoom_(zoom),
      shorthand_scale_(1),
      current_color_(current_color),
      color_scheme_(color_scheme),
      fill_flags_(fill_flags),
      stroke_flags_(stroke_flags) {}

FilterEffect* FilterEffectBuilder::BuildFilterEffect(
    const FilterOperations& operations,
    bool input_tainted) const {
  // Create a parent filter for shorthand filters. These have already been
  // scaled by the CSS code for page zoom, so scale is 1.0 here.
  auto* parent_filter = MakeGarbageCollected<Filter>(1.0f);
  FilterEffect* previous_effect = parent_filter->GetSourceGraphic();
  if (input_tainted)
    previous_effect->SetOriginTainted();
  for (FilterOperation* filter_operation : operations.Operations()) {
    FilterEffect* effect = nullptr;
    switch (filter_operation->GetType()) {
      case FilterOperation::OperationType::kReference: {
        auto& reference_operation =
            To<ReferenceFilterOperation>(*filter_operation);
        Filter* reference_filter =
            BuildReferenceFilter(reference_operation, previous_effect);
        if (reference_filter) {
          effect = reference_filter->LastEffect();
          // TODO(fs): This is essentially only needed for the
          // side-effects (mapRect). The filter differs from the one
          // computed just above in what the SourceGraphic is, and how
          // it's connected to the filter-chain.
          reference_filter = BuildReferenceFilter(reference_operation, nullptr);
        }
        reference_operation.SetFilter(reference_filter);
        break;
      }
      case FilterOperation::OperationType::kGrayscale: {
        Vector<float> input_parameters = GrayscaleMatrix(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount());
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_MATRIX,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kSepia: {
        Vector<float> input_parameters = SepiaMatrix(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount());
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_MATRIX,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kSaturate: {
        Vector<float> input_parameters;
        input_parameters.push_back(ClampTo<float>(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount()));
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_SATURATE,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kHueRotate: {
        Vector<float> input_parameters;
        input_parameters.push_back(ClampTo<float>(
            To<BasicColorMatrixFilterOperation>(filter_operation)->Amount()));
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_HUEROTATE,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kLuminanceToAlpha: {
        Vector<float> input_parameters;
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_LUMINANCETOALPHA,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kColorMatrix: {
        Vector<float> input_parameters =
            To<ColorMatrixFilterOperation>(filter_operation)->Values();
        effect = MakeGarbageCollected<FEColorMatrix>(
            parent_filter, FECOLORMATRIX_TYPE_MATRIX,
            std::move(input_parameters));
        break;
      }
      case FilterOperation::OperationType::kInvert: {
        BasicComponentTransferFilterOperation* component_transfer_operation =
            To<BasicComponentTransferFilterOperation>(filter_operation);
        ComponentTransferFunction transfer_function;
        transfer_function.type = FECOMPONENTTRANSFER_TYPE_TABLE;
        Vector<float> transfer_parameters;
        transfer_parameters.push_back(
            ClampTo<float>(component_transfer_operation->Amount()));
        transfer_parameters.push_back(
            ClampTo<float>(1 - component_transfer_operation->Amount()));
        transfer_function.table_values = transfer_parameters;

        ComponentTransferFunction null_function;
        effect = MakeGarbageCollected<FEComponentTransfer>(
            parent_filter, transfer_function, transfer_function,
            transfer_function, null_function);
        break;
      }
      case FilterOperation::OperationType::kOpacity: {
        ComponentTransferFunction transfer_function;
        transfer_function.type = FECOMPONENTTRANSFER_TYPE_TABLE;
        Vector<float> transfer_parameters;
        transfer_parameters.push_back(0);
        transfer_parameters.push_back(ClampTo<float>(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount()));
        transfer_function.table_values = transfer_parameters;

        ComponentTransferFunction null_function;
        effect = MakeGarbageCollected<FEComponentTransfer>(
            parent_filter, null_function, null_function, null_function,
            transfer_function);
        break;
      }
      case FilterOperation::OperationType::kBrightness: {
        ComponentTransferFunction transfer_function;
        transfer_function.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
        transfer_function.slope = ClampTo<float>(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount());
        transfer_function.intercept = 0;

        ComponentTransferFunction null_function;
        effect = MakeGarbageCollected<FEComponentTransfer>(
            parent_filter, transfer_function, transfer_function,
            transfer_function, null_function);
        break;
      }
      case FilterOperation::OperationType::kContrast: {
        ComponentTransferFunction transfer_function;
        transfer_function.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
        float amount = ClampTo<float>(
            To<BasicComponentTransferFilterOperation>(filter_operation)
                ->Amount());
        transfer_function.slope = amount;
        transfer_function.intercept = -0.5 * amount + 0.5;

        ComponentTransferFunction null_function;
        effect = MakeGarbageCollected<FEComponentTransfer>(
            parent_filter, transfer_function, transfer_function,
            transfer_function, null_function);
        break;
      }
      case FilterOperation::OperationType::kBlur: {
        const LengthPoint& std_deviation =
            To<BlurFilterOperation>(filter_operation)->StdDeviationXY();
        effect = MakeGarbageCollected<FEGaussianBlur>(
            parent_filter,
            FloatValueForLength(std_deviation.X(), 0) * shorthand_scale_,
            FloatValueForLength(std_deviation.Y(), 0) * shorthand_scale_);
        break;
      }
      case FilterOperation::OperationType::kDropShadow: {
        const ShadowData& shadow =
            To<DropShadowFilterOperation>(*filter_operation).Shadow();
        const gfx::Vector2dF offset =
            gfx::ScaleVector2d(shadow.Offset(), shorthand_scale_);
        gfx::PointF blur = gfx::ScalePoint(shadow.BlurXY(), shorthand_scale_);
        effect = MakeGarbageCollected<FEDropShadow>(
            parent_filter, blur.x(), blur.y(), offset.x(), offset.y(),
            shadow.GetColor().Resolve(current_color_, color_scheme_),
            shadow.Opacity());
        if (shadow.GetColor().IsCurrentColor()) {
          effect->SetOriginTainted();
        }
        break;
      }
      case FilterOperation::OperationType::kBoxReflect: {
        BoxReflectFilterOperation* box_reflect_operation =
            To<BoxReflectFilterOperation>(filter_operation);
        effect = MakeGarbageCollected<FEBoxReflect>(
            parent_filter, box_reflect_operation->Reflection());
        break;
      }
      case FilterOperation::OperationType::kConvolveMatrix: {
        ConvolveMatrixFilterOperation* convolve_matrix_operation =
            To<ConvolveMatrixFilterOperation>(filter_operation);
        effect = MakeGarbageCollected<FEConvolveMatrix>(
            parent_filter, convolve_matrix_operation->KernelSize(),
            convolve_matrix_operation->Divisor(),
            convolve_matrix_operation->Bias(),
            convolve_matrix_operation->TargetOffset().OffsetFromOrigin(),
            convolve_matrix_operation->EdgeMode(),
            convolve_matrix_operation->PreserveAlpha(),
            convolve_matrix_operation->KernelMatrix());
        break;
      }
      case FilterOperation::OperationType::kComponentTransfer: {
        ComponentTransferFilterOperation* component_transfer_operation =
            To<ComponentTransferFilterOperation>(filter_operation);
        effect = MakeGarbageCollected<FEComponentTransfer>(
            parent_filter, component_transfer_operation->RedFunc(),
            component_transfer_operation->GreenFunc(),
            component_transfer_operation->BlueFunc(),
            component_transfer_operation->AlphaFunc());
        break;
      }
      case FilterOperation::OperationType::kTurbulence: {
        TurbulenceFilterOperation* turbulence_filter_operation =
            To<TurbulenceFilterOperation>(filter_operation);
        effect = MakeGarbageCollected<FETurbulence>(
            parent_filter, turbulence_filter_operation->Type(),
            turbulence_filter_operation->BaseFrequencyX(),
            turbulence_filter_operation->BaseFrequencyY(),
            turbulence_filter_operation->NumOctaves(),
            turbulence_filter_operation->Seed(),
            turbulence_filter_operation->StitchTiles());
        break;
      }
      default:
        break;
    }

    if (effect) {
      if (filter_operation->GetType() !=
          FilterOperation::OperationType::kReference) {
        // Unlike SVG, filters applied here should not clip to their primitive
        // subregions.
        effect->SetClipsToBounds(false);
        effect->SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
        effect->InputEffects().push_back(previous_effect);
      }
      if (previous_effect->OriginTainted())
        effect->SetOriginTainted();
      previous_effect = effect;
    }
  }
  return previous_effect;
}

CompositorFilterOperations FilterEffectBuilder::BuildFilterOperations(
    const FilterOperations& operations) const {
  InterpolationSpace current_interpolation_space = kInterpolationSpaceSRGB;

  CompositorFilterOperations filters;
  for (FilterOperation* op : operations.Operations()) {
    switch (op->GetType()) {
      case FilterOperation::OperationType::kReference: {
        auto& reference_operation = To<ReferenceFilterOperation>(*op);
        Filter* reference_filter =
            BuildReferenceFilter(reference_operation, nullptr);
        if (reference_filter && reference_filter->LastEffect()) {
          // Set the interpolation space for the source of the (sub)filter to
          // match that of the previous primitive (or input).
          auto* source = reference_filter->GetSourceGraphic();
          source->SetOperatingInterpolationSpace(current_interpolation_space);
          paint_filter_builder::PopulateSourceGraphicImageFilters(
              source, current_interpolation_space);

          FilterEffect* filter_effect = reference_filter->LastEffect();
          current_interpolation_space =
              filter_effect->OperatingInterpolationSpace();
          auto paint_filter = paint_filter_builder::Build(
              filter_effect, current_interpolation_space);
          if (!paint_filter)
            continue;
          filters.AppendReferenceFilter(std::move(paint_filter));
        }
        reference_operation.SetFilter(reference_filter);
        break;
      }
      case FilterOperation::OperationType::kGrayscale:
      case FilterOperation::OperationType::kSepia:
      case FilterOperation::OperationType::kSaturate:
      case FilterOperation::OperationType::kHueRotate: {
        float amount = To<BasicColorMatrixFilterOperation>(*op).Amount();
        switch (op->GetType()) {
          case FilterOperation::OperationType::kGrayscale:
            filters.AppendGrayscaleFilter(amount);
            break;
          case FilterOperation::OperationType::kSepia:
            filters.AppendSepiaFilter(amount);
            break;
          case FilterOperation::OperationType::kSaturate:
            filters.AppendSaturateFilter(amount);
            break;
          case FilterOperation::OperationType::kHueRotate:
            filters.AppendHueRotateFilter(amount);
            break;
          default:
            NOTREACHED_IN_MIGRATION();
        }
        break;
      }
      case FilterOperation::OperationType::kLuminanceToAlpha:
      case FilterOperation::OperationType::kConvolveMatrix:
      case FilterOperation::OperationType::kComponentTransfer:
      case FilterOperation::OperationType::kTurbulence:
        // These filter types only exist for Canvas filters.
        NOTREACHED_IN_MIGRATION();
        break;
      case FilterOperation::OperationType::kColorMatrix: {
        Vector<float> matrix_values =
            To<ColorMatrixFilterOperation>(*op).Values();
        filters.AppendColorMatrixFilter(std::move(matrix_values));
        break;
      }
      case FilterOperation::OperationType::kInvert:
      case FilterOperation::OperationType::kOpacity:
      case FilterOperation::OperationType::kBrightness:
      case FilterOperation::OperationType::kContrast: {
        float amount = To<BasicComponentTransferFilterOperation>(*op).Amount();
        switch (op->GetType()) {
          case FilterOperation::OperationType::kInvert:
            filters.AppendInvertFilter(amount);
            break;
          case FilterOperation::OperationType::kOpacity:
            filters.AppendOpacityFilter(amount);
            break;
          case FilterOperation::OperationType::kBrightness:
            filters.AppendBrightnessFilter(amount);
            break;
          case FilterOperation::OperationType::kContrast:
            filters.AppendContrastFilter(amount);
            break;
          default:
            NOTREACHED_IN_MIGRATION();
        }
        break;
      }
      case FilterOperation::OperationType::kBlur: {
        float pixel_radius =
            To<BlurFilterOperation>(*op).StdDeviation().GetFloatValue();
        pixel_radius *= shorthand_scale_;
        filters.AppendBlurFilter(pixel_radius);
        break;
      }
      case FilterOperation::OperationType::kDropShadow: {
        const ShadowData& shadow = To<DropShadowFilterOperation>(*op).Shadow();
        const gfx::Vector2d floored_offset = gfx::ToFlooredVector2d(
            gfx::ScaleVector2d(shadow.Offset(), shorthand_scale_));
        float radius = shadow.Blur() * shorthand_scale_;
        filters.AppendDropShadowFilter(
            floored_offset, radius,
            shadow.GetColor().Resolve(current_color_, color_scheme_));
        break;
      }
      case FilterOperation::OperationType::kBoxReflect: {
        // TODO(jbroman): Consider explaining box reflect to the compositor,
        // instead of calling this a "reference filter".
        const auto& reflection =
            To<BoxReflectFilterOperation>(*op).Reflection();
        filters.AppendReferenceFilter(
            paint_filter_builder::BuildBoxReflectFilter(reflection, nullptr));
        break;
      }
      case FilterOperation::OperationType::kNone:
        break;
    }
    // TODO(fs): When transitioning from a reference filter using "linearRGB"
    // to a filter function we should insert a conversion (like the one below)
    // for the results to be correct.
  }
  if (current_interpolation_space != kInterpolationSpaceSRGB) {
    // Transform to device color space at the end of processing, if required.
    sk_sp<PaintFilter> filter =
        paint_filter_builder::TransformInterpolationSpace(
            nullptr, current_interpolation_space, kInterpolationSpaceSRGB);
    filters.AppendReferenceFilter(std::move(filter));
  }

  if (!filters.IsEmpty())
    filters.SetReferenceBox(reference_box_);

  return filters;
}

Filter* FilterEffectBuilder::BuildReferenceFilter(
    const ReferenceFilterOperation& reference_operation,
    FilterEffect* previous_effect,
    SVGFilterGraphNodeMap* node_map) const {
  SVGResource* resource = reference_operation.Resource();
  auto* filter_element =
      DynamicTo<SVGFilterElement>(resource ? resource->Target() : nullptr);
  if (!filter_element)
    return nullptr;
  if (auto* resource_container = resource->ResourceContainerNoCycleCheck())
    resource_container->ClearInvalidationMask();

  std::optional<gfx::SizeF> unzoomed_viewport;
  if (viewport_) {
    gfx::SizeF unzoomed = *viewport_;
    unzoomed.InvScale(zoom_);
    unzoomed_viewport = unzoomed;
  }

  gfx::RectF filter_region =
      LayoutSVGResourceContainer::ResolveRectangle<SVGFilterElement>(
          *filter_element, filter_element->filterUnits()->CurrentEnumValue(),
          reference_box_, unzoomed_viewport);
  bool primitive_bounding_box_mode =
      filter_element->primitiveUnits()->CurrentEnumValue() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox;
  Filter::UnitScaling unit_scaling =
      primitive_bounding_box_mode ? Filter::kBoundingBox : Filter::kUserSpace;
  auto* result = MakeGarbageCollected<Filter>(reference_box_, filter_region,
                                              zoom_, unit_scaling);
  // If the filter has an empty region, then return a Filter without any
  // primitives since the behavior in these two cases (no primitives, empty
  // region) should match.
  if (filter_region.IsEmpty()) {
    // TODO(fs): We rely on the presence of a node map here to opt-in to the
    // check for an empty filter region. The reason for this is that we lack a
    // viewport to resolve against for HTML content. This is crbug.com/512453.
    if (viewport_ || node_map) {
      return result;
    }
  }

  if (!previous_effect)
    previous_effect = result->GetSourceGraphic();
  SVGFilterBuilder builder(previous_effect, node_map, fill_flags_,
                           stroke_flags_);
  builder.BuildGraph(result, *filter_element, reference_box_);
  result->SetLastEffect(builder.LastEffect());
  return result;
}

}  // namespace blink
