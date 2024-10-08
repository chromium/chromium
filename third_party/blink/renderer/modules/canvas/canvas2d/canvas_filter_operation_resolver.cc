// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/types/expected.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_fe_turbulence_element.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_component_transfer.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_convolve_matrix.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_turbulence.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class Font;

namespace {
int num_canvas_filter_errors_to_console_allowed_ = 64;

ColorMatrixFilterOperation* ResolveColorMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  std::optional<Vector<float>> values =
      dict.Get<IDLSequence<IDLFloat>>("values", exception_state);

  if (!values.has_value()) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' array required.");
    return nullptr;
  }

  if (values->size() != 20) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' must be an array "
        "of 20 numbers.");
    return nullptr;
  }

  return MakeGarbageCollected<ColorMatrixFilterOperation>(
      *std::move(values), FilterOperation::OperationType::kColorMatrix);
}

struct KernelMatrix {
  Vector<float> values;
  uint32_t width;
  uint32_t height;
};

// For resolving feConvolveMatrix type filters
std::optional<KernelMatrix> GetKernelMatrix(const Dictionary& dict,
                                            ExceptionState& exception_state) {
  std::optional<Vector<Vector<float>>> km_input =
      dict.Get<IDLSequence<IDLSequence<IDLFloat>>>("kernelMatrix",
                                                   exception_state);
  if (!km_input.has_value() || km_input->size() == 0 ||
      (km_input->size() >= 2 && km_input->at(0).size() == 0)) {
    exception_state.ThrowTypeError(
        "Failed to construct convolve matrix filter. 'kernelMatrix' must be an "
        "array of arrays of numbers representing an n by m matrix.");
    return std::nullopt;
  }
  KernelMatrix result;
  result.height = km_input->size();
  result.width = km_input->at(0).size();

  for (const Vector<float>& row : *km_input) {
    if (row.size() != result.width) {
      exception_state.ThrowTypeError(
          "Failed to construct convolve matrix filter. All rows of the "
          "'kernelMatrix' must be the same length.");
      return std::nullopt;
    }

    result.values.AppendVector(row);
  }

  return result;
}

ConvolveMatrixFilterOperation* ResolveConvolveMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  std::optional<KernelMatrix> kernel_matrix =
      GetKernelMatrix(dict, exception_state);

  if (!kernel_matrix.has_value()) {
    return nullptr;
  }

  gfx::Size kernel_size(kernel_matrix->width, kernel_matrix->height);
  double divisor = dict.Get<IDLDouble>("divisor", exception_state).value_or(1);
  double bias = dict.Get<IDLDouble>("bias", exception_state).value_or(0);
  gfx::Point target_offset =
      gfx::Point(dict.Get<IDLShort>("targetX", exception_state)
                     .value_or(kernel_matrix->width / 2),
                 dict.Get<IDLShort>("targetY", exception_state)
                     .value_or(kernel_matrix->height / 2));

  String edge_mode_string =
      dict.Get<IDLString>("edgeMode", exception_state).value_or("duplicate");
  FEConvolveMatrix::EdgeModeType edge_mode =
      static_cast<FEConvolveMatrix::EdgeModeType>(
          GetEnumerationMap<FEConvolveMatrix::EdgeModeType>().ValueFromName(
              edge_mode_string));

  bool preserve_alpha =
      dict.Get<IDLBoolean>("preserveAlpha", exception_state).value_or(false);

  return MakeGarbageCollected<ConvolveMatrixFilterOperation>(
      kernel_size, divisor, bias, target_offset, edge_mode, preserve_alpha,
      kernel_matrix->values);
}

ComponentTransferFunction GetComponentTransferFunction(
    const StringView& key,
    const Dictionary& filter,
    ExceptionState& exception_state) {
  ComponentTransferFunction result;
  // An earlier stage threw an error
  if (exception_state.HadException())
    return result;
  Dictionary transfer_dict;
  filter.Get(key, transfer_dict);

  result.slope =
      transfer_dict.Get<IDLDouble>("slope", exception_state).value_or(1);
  result.intercept =
      transfer_dict.Get<IDLDouble>("intercept", exception_state).value_or(0);
  result.amplitude =
      transfer_dict.Get<IDLDouble>("amplitude", exception_state).value_or(1);
  result.exponent =
      transfer_dict.Get<IDLDouble>("exponent", exception_state).value_or(1);
  result.offset =
      transfer_dict.Get<IDLDouble>("offset", exception_state).value_or(0);

  String type = transfer_dict.Get<IDLString>("type", exception_state)
                    .value_or("identity");
  if (type == "identity")
    result.type = FECOMPONENTTRANSFER_TYPE_IDENTITY;
  else if (type == "linear")
    result.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
  else if (type == "gamma")
    result.type = FECOMPONENTTRANSFER_TYPE_GAMMA;
  else if (type == "table")
    result.type = FECOMPONENTTRANSFER_TYPE_TABLE;
  else if (type == "discrete")
    result.type = FECOMPONENTTRANSFER_TYPE_DISCRETE;

  std::optional<Vector<float>> table_values =
      transfer_dict.Get<IDLSequence<IDLFloat>>("tableValues", exception_state);
  if (table_values.has_value()) {
    result.table_values.AppendVector(*table_values);
  }

  return result;
}

ComponentTransferFilterOperation* ResolveComponentTransfer(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ComponentTransferFilterOperation>(
      GetComponentTransferFunction("funcR", dict, exception_state),
      GetComponentTransferFunction("funcG", dict, exception_state),
      GetComponentTransferFunction("funcB", dict, exception_state),
      GetComponentTransferFunction("funcA", dict, exception_state));
}

StyleColor ResolveFloodColor(ExecutionContext& execution_context,
                             const Dictionary& dict,
                             ExceptionState& exception_state) {
  NonThrowableExceptionState no_throw;
  if (!dict.HasProperty("floodColor", no_throw)) {
    return StyleColor(Color::kBlack);
  }

  // TODO(crbug.com/1430532): CurrentColor and system colors dependeing on
  // the color-scheme should be stored unresolved, and resolved only when the
  // filter is associated with a context.
  std::optional<String> flood_color =
      dict.Get<IDLString>("floodColor", exception_state);
  Color parsed_color;
  if (exception_state.HadException() || !flood_color.has_value() ||
      !ParseCanvasColorString(*flood_color, parsed_color)) {
    exception_state.ThrowTypeError(
        "Invalid color value for \"floodColor\" property.");
    return StyleColor(Color::kBlack);
  }

  return StyleColor(parsed_color);
}

base::expected<gfx::PointF, String> ResolveFloatOrVec2f(
    const String property_name,
    const Dictionary& dict,
    ExceptionState& exception_state) {
  {
    v8::TryCatch try_catch(dict.GetIsolate());
    // First try to get stdDeviation as a float.
    std::optional<float> single_float = dict.Get<IDLFloat>(
        property_name, PassThroughException(dict.GetIsolate()));
    if (!try_catch.HasCaught() && single_float.has_value()) {
      return gfx::PointF(*single_float, *single_float);
    }
  }
  // Try again as a vector.
  std::optional<Vector<float>> two_floats =
      dict.Get<IDLSequence<IDLFloat>>(property_name, exception_state);
  if (exception_state.HadException() || !two_floats.has_value() ||
      two_floats->size() != 2) {
    return base::unexpected(String::Format(
        "\"%s\" must either be a number or an array of two numbers",
        property_name.Ascii().c_str()));
  }
  return gfx::PointF(two_floats->at(0), two_floats->at(1));
}

BlurFilterOperation* ResolveBlur(const Dictionary& blur_dict,
                                 ExceptionState& exception_state) {
  base::expected<gfx::PointF, String> blur_xy =
      ResolveFloatOrVec2f("stdDeviation", blur_dict, exception_state);

  if (exception_state.HadException() || !blur_xy.has_value()) {
    exception_state.ThrowTypeError(
        String::Format("Failed to construct blur filter. %s.",
                       blur_xy.error().Utf8().c_str()));
    return nullptr;
  }

  return MakeGarbageCollected<BlurFilterOperation>(
      Length::Fixed(std::max(0.0f, blur_xy->x())),
      Length::Fixed(std::max(0.0f, blur_xy->y())));
}

DropShadowFilterOperation* ResolveDropShadow(
    ExecutionContext& execution_context,
    const Dictionary& dict,
    ExceptionState& exception_state) {
  // For checking the presence of keys.
  NonThrowableExceptionState no_throw;

  float dx = 2.0f;
  if (dict.HasProperty("dx", no_throw)) {
    std::optional<float> input = dict.Get<IDLFloat>("dx", exception_state);
    if (exception_state.HadException() || !input.has_value()) {
      exception_state.ThrowTypeError(
          "Failed to construct dropShadow filter, \"dx\" must be a number.");
      return nullptr;
    }
    dx = *input;
  }

  float dy = 2.0f;
  if (dict.HasProperty("dy", no_throw)) {
    std::optional<float> input = dict.Get<IDLFloat>("dy", exception_state);
    if (exception_state.HadException() || !input.has_value()) {
      exception_state.ThrowTypeError(
          "Failed to construct dropShadow filter, \"dy\" must be a number.");
      return nullptr;
    }
    dy = *input;
  }

  // The shadow blur can have different standard deviations in the X and Y
  // directions. `stdDeviation` can be specified as either a single number
  // (same X & Y blur) or a vector of two numbers (different X & Y blurs).
  gfx::PointF blur = {2.0f, 2.0f};
  if (dict.HasProperty("stdDeviation", no_throw)) {
    base::expected<gfx::PointF, String> std_deviation =
        ResolveFloatOrVec2f("stdDeviation", dict, exception_state);
    if (exception_state.HadException() || !std_deviation.has_value()) {
      exception_state.ThrowTypeError(
          String::Format("Failed to construct dropShadow filter, %s.",
                         std_deviation.error().Utf8().c_str()));
      return nullptr;
    }
    blur = *std_deviation;
    blur.SetToMax({0.0f, 0.0f});
  }

  StyleColor flood_color =
      ResolveFloodColor(execution_context, dict, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  float opacity = 1.0f;
  if (dict.HasProperty("floodOpacity", no_throw)) {
    std::optional<float> input =
        dict.Get<IDLFloat>("floodOpacity", exception_state);
    if (exception_state.HadException() || !input.has_value()) {
      exception_state.ThrowTypeError(
          "Failed to construct dropShadow filter, \"floodOpacity\" must be a "
          "number.");
      return nullptr;
    }
    opacity = *input;
  }

  return MakeGarbageCollected<DropShadowFilterOperation>(
      ShadowData(gfx::Vector2dF(dx, dy), blur, /*spread=*/0,
                 ShadowStyle::kNormal, std::move(flood_color), opacity));
}

// https://drafts.fxtf.org/filter-effects/#feTurbulenceElement
TurbulenceFilterOperation* ResolveTurbulence(const Dictionary& dict,
                                             ExceptionState& exception_state) {
  // Default values for all parameters per spec.
  float base_frequency_x = 0;
  float base_frequency_y = 0;
  float seed = 1;
  int num_octaves = 1;
  SVGStitchOptions stitch_tiles = kSvgStitchtypeNostitch;
  TurbulenceType type = FETURBULENCE_TYPE_TURBULENCE;

  // For checking the presence of keys.
  NonThrowableExceptionState no_throw;

  // baseFrequency can be either a number or a list of numbers.
  if (dict.HasProperty("baseFrequency", no_throw)) {
    base::expected<gfx::PointF, String> base_frequency =
        ResolveFloatOrVec2f("baseFrequency", dict, exception_state);
    if (exception_state.HadException() || !base_frequency.has_value()) {
      exception_state.ThrowTypeError(
          String::Format("Failed to construct turbulence filter, %s.",
                         base_frequency.error().Utf8().c_str()));
      return nullptr;
    }
    base_frequency_x = base_frequency->x();
    base_frequency_y = base_frequency->y();

    if (base_frequency_x < 0 || base_frequency_y < 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, negative values for "
          "\"baseFrequency\" are unsupported.");
      return nullptr;
    }
  }

  if (dict.HasProperty("seed", no_throw)) {
    std::optional<float> seed_input =
        dict.Get<IDLFloat>("seed", exception_state);
    if (exception_state.HadException() || !seed_input.has_value()) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"seed\" must be a number.");
      return nullptr;
    }
    seed = *seed_input;
  }

  if (dict.HasProperty("numOctaves", no_throw)) {
    // Get numOctaves as a float and then cast to int so that we throw for
    // inputs like undefined, NaN and Infinity.
    std::optional<float> num_octaves_input =
        dict.Get<IDLFloat>("numOctaves", exception_state);
    if (exception_state.HadException() || !num_octaves_input.has_value() ||
        *num_octaves_input < 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"numOctaves\" must be a "
          "positive number.");
      return nullptr;
    }
    num_octaves = static_cast<int>(*num_octaves_input);
  }

  if (dict.HasProperty("stitchTiles", no_throw)) {
    std::optional<String> stitch_tiles_input =
        dict.Get<IDLString>("stitchTiles", exception_state);
    if (exception_state.HadException() || !stitch_tiles_input.has_value() ||
        (stitch_tiles = static_cast<SVGStitchOptions>(
             GetEnumerationMap<SVGStitchOptions>().ValueFromName(
                 *stitch_tiles_input))) == 0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"stitchTiles\" must be "
          "either \"stitch\" or \"noStitch\".");
      return nullptr;
    }
  }

  if (dict.HasProperty("type", no_throw)) {
    std::optional<String> type_input =
        dict.Get<IDLString>("type", exception_state);
    if (exception_state.HadException() || !type_input.has_value() ||
        (type = static_cast<TurbulenceType>(
             GetEnumerationMap<TurbulenceType>().ValueFromName(*type_input))) ==
            0) {
      exception_state.ThrowTypeError(
          "Failed to construct turbulence filter, \"type\" must be either "
          "\"turbulence\" or \"fractalNoise\".");
      return nullptr;
    }
  }

  return MakeGarbageCollected<TurbulenceFilterOperation>(
      type, base_frequency_x, base_frequency_y, num_octaves, seed,
      stitch_tiles == kSvgStitchtypeStitch ? true : false);
}

}  // namespace

FilterOperations CanvasFilterOperationResolver::CreateFilterOperationsFromList(
    const HeapVector<ScriptValue>& filters,
    ExecutionContext& execution_context,
    ExceptionState& exception_state) {
  FilterOperations operations;
  for (auto filter : filters) {
    Dictionary filter_dict = Dictionary(filter);
    std::optional<String> name =
        filter_dict.Get<IDLString>("name", exception_state);
    if (name == "gaussianBlur") {
      if (auto* blur_operation = ResolveBlur(filter_dict, exception_state)) {
        operations.Operations().push_back(blur_operation);
      }
    } else if (name == "colorMatrix") {
      String type = filter_dict.Get<IDLString>("type", exception_state)
                        .value_or("matrix");
      if (type == "hueRotate") {
        double amount =
            filter_dict.Get<IDLDouble>("values", exception_state).value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::OperationType::kHueRotate));
      } else if (type == "saturate") {
        double amount =
            filter_dict.Get<IDLDouble>("values", exception_state).value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::OperationType::kSaturate));
      } else if (type == "luminanceToAlpha") {
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                0, FilterOperation::OperationType::kLuminanceToAlpha));
      } else if (auto* color_matrix_operation =
                     ResolveColorMatrix(filter_dict, exception_state)) {
        operations.Operations().push_back(color_matrix_operation);
      }
    } else if (name == "convolveMatrix") {
      if (auto* convolve_operation =
              ResolveConvolveMatrix(filter_dict, exception_state)) {
        operations.Operations().push_back(convolve_operation);
      }
    } else if (name == "componentTransfer") {
      if (auto* component_transfer_operation =
              ResolveComponentTransfer(filter_dict, exception_state)) {
        operations.Operations().push_back(component_transfer_operation);
      }
    } else if (name == "dropShadow") {
      if (FilterOperation* drop_shadow_operation = ResolveDropShadow(
              execution_context, filter_dict, exception_state)) {
        operations.Operations().push_back(drop_shadow_operation);
      }
    } else if (name == "turbulence") {
      if (auto* turbulence_operation =
              ResolveTurbulence(filter_dict, exception_state)) {
        operations.Operations().push_back(turbulence_operation);
      }
    } else {
      num_canvas_filter_errors_to_console_allowed_--;
      if (num_canvas_filter_errors_to_console_allowed_ < 0)
        continue;
      {
        const String& message =
            (!name.has_value())
                ? "Canvas filter require key 'name' to specify filter type."
                : String::Format(
                      "\"%s\" is not among supported canvas filter types.",
                      name->Utf8().c_str());
        execution_context.AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kRendering,
                mojom::blink::ConsoleMessageLevel::kWarning, message));
      }
      if (num_canvas_filter_errors_to_console_allowed_ == 0) {
        const String& message =
            "Canvas filter: too many errors, no more errors will be reported "
            "to the console for this process.";
        execution_context.AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kRendering,
                mojom::blink::ConsoleMessageLevel::kWarning, message));
      }
    }
  }

  return operations;
}

FilterOperations
CanvasFilterOperationResolver::CreateFilterOperationsFromCSSFilter(
    const String& filter_string,
    const ExecutionContext& execution_context,
    Element* style_resolution_host,
    const Font& font) {
  FilterOperations operations;
  const CSSValue* css_value = CSSParser::ParseSingleValue(
      CSSPropertyID::kFilter, filter_string,
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, execution_context.GetSecureContextMode()));
  if (!css_value || css_value->IsCSSWideKeyword()) {
    return operations;
  }
  // The style resolution for fonts is not available in frame-less documents.
  if (style_resolution_host != nullptr &&
      style_resolution_host->GetDocument().GetFrame() != nullptr) {
    return style_resolution_host->GetDocument()
        .GetStyleResolver()
        .ComputeFilterOperations(style_resolution_host, font, *css_value);
  } else {
    return FilterOperationResolver::CreateOffscreenFilterOperations(*css_value,
                                                                    font);
  }
}

}  // namespace blink
