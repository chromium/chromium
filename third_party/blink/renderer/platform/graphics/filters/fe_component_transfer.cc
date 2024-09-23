/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/filters/fe_component_transfer.h"

#include <algorithm>

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

typedef void (*TransferType)(unsigned char*, const ComponentTransferFunction&);

FEComponentTransfer::FEComponentTransfer(
    Filter* filter,
    const ComponentTransferFunction& red_func,
    const ComponentTransferFunction& green_func,
    const ComponentTransferFunction& blue_func,
    const ComponentTransferFunction& alpha_func)
    : FilterEffect(filter),
      red_func_(red_func),
      green_func_(green_func),
      blue_func_(blue_func),
      alpha_func_(alpha_func) {}

static void Identity(unsigned char*, const ComponentTransferFunction&) {}

static void Table(unsigned char* values,
                  const ComponentTransferFunction& transfer_function) {
  const Vector<float>& table_values = transfer_function.table_values;
  unsigned n = table_values.size();
  if (n < 1)
    return;
  for (unsigned i = 0; i < 256; ++i) {
    double c = i / 255.0;
    unsigned k = static_cast<unsigned>(c * (n - 1));
    double v1 = table_values[k];
    double v2 = table_values[std::min((k + 1), (n - 1))];
    double val = 255.0 * (v1 + (c * (n - 1) - k) * (v2 - v1));
    val = ClampTo(val, 0.0, 255.0);
    values[i] = static_cast<unsigned char>(val);
  }
}

static void Discrete(unsigned char* values,
                     const ComponentTransferFunction& transfer_function) {
  const Vector<float>& table_values = transfer_function.table_values;
  unsigned n = table_values.size();
  if (n < 1)
    return;
  for (unsigned i = 0; i < 256; ++i) {
    unsigned k = static_cast<unsigned>((i * n) / 255.0);
    k = std::min(k, n - 1);
    double val = 255 * table_values[k];
    val = ClampTo(val, 0.0, 255.0);
    values[i] = static_cast<unsigned char>(val);
  }
}

static void Linear(unsigned char* values,
                   const ComponentTransferFunction& transfer_function) {
  for (unsigned i = 0; i < 256; ++i) {
    double val =
        transfer_function.slope * i + 255 * transfer_function.intercept;
    val = ClampTo(val, 0.0, 255.0);
    values[i] = static_cast<unsigned char>(val);
  }
}

static void Gamma(unsigned char* values,
                  const ComponentTransferFunction& transfer_function) {
  for (unsigned i = 0; i < 256; ++i) {
    double exponent = transfer_function.exponent;
    double val =
        255.0 * (transfer_function.amplitude * pow((i / 255.0), exponent) +
                 transfer_function.offset);
    val = ClampTo(val, 0.0, 255.0);
    values[i] = static_cast<unsigned char>(val);
  }
}

bool FEComponentTransfer::AffectsTransparentPixels() const {
  double intercept = 0;
  switch (alpha_func_.type) {
    case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
    case FECOMPONENTTRANSFER_TYPE_IDENTITY:
      break;
    case FECOMPONENTTRANSFER_TYPE_TABLE:
    case FECOMPONENTTRANSFER_TYPE_DISCRETE:
      if (alpha_func_.table_values.size() > 0)
        intercept = alpha_func_.table_values[0];
      break;
    case FECOMPONENTTRANSFER_TYPE_LINEAR:
      intercept = alpha_func_.intercept;
      break;
    case FECOMPONENTTRANSFER_TYPE_GAMMA:
      intercept = alpha_func_.offset;
      break;
  }
  return 255 * intercept >= 1;
}

sk_sp<PaintFilter> FEComponentTransfer::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));

  unsigned char r_values[256], g_values[256], b_values[256], a_values[256];
  GetValues(r_values, g_values, b_values, a_values);

  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  sk_sp<cc::ColorFilter> color_filter =
      cc::ColorFilter::MakeTableARGB(a_values, r_values, g_values, b_values);
  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter),
                                            std::move(input),
                                            base::OptionalToPtr(crop_rect));
}

void FEComponentTransfer::GetValues(unsigned char r_values[256],
                                    unsigned char g_values[256],
                                    unsigned char b_values[256],
                                    unsigned char a_values[256]) {
  for (unsigned i = 0; i < 256; ++i)
    r_values[i] = g_values[i] = b_values[i] = a_values[i] = i;
  unsigned char* tables[] = {r_values, g_values, b_values, a_values};
  ComponentTransferFunction transfer_function[] = {red_func_, green_func_,
                                                   blue_func_, alpha_func_};
  TransferType call_effect[] = {Identity, Identity, Table,
                                Discrete, Linear,   Gamma};

  for (unsigned channel = 0; channel < 4; channel++) {
    SECURITY_DCHECK(static_cast<size_t>(transfer_function[channel].type) <
                    std::size(call_effect));
    (*call_effect[transfer_function[channel].type])(tables[channel],
                                                    transfer_function[channel]);
  }
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const ComponentTransferType& type) {
  switch (type) {
    case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FECOMPONENTTRANSFER_TYPE_IDENTITY:
      ts << "IDENTITY";
      break;
    case FECOMPONENTTRANSFER_TYPE_TABLE:
      ts << "TABLE";
      break;
    case FECOMPONENTTRANSFER_TYPE_DISCRETE:
      ts << "DISCRETE";
      break;
    case FECOMPONENTTRANSFER_TYPE_LINEAR:
      ts << "LINEAR";
      break;
    case FECOMPONENTTRANSFER_TYPE_GAMMA:
      ts << "GAMMA";
      break;
  }
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const ComponentTransferFunction& function) {
  ts << "type=\"" << function.type << "\" slope=\"" << function.slope
     << "\" intercept=\"" << function.intercept << "\" amplitude=\""
     << function.amplitude << "\" exponent=\"" << function.exponent
     << "\" offset=\"" << function.offset << "\"";
  return ts;
}

WTF::TextStream& FEComponentTransfer::ExternalRepresentation(
    WTF::TextStream& ts,
    int indent) const {
  WriteIndent(ts, indent);
  ts << "[feComponentTransfer";
  FilterEffect::ExternalRepresentation(ts);
  ts << " \n";
  WriteIndent(ts, indent + 2);
  ts << "{red: " << red_func_ << "}\n";
  WriteIndent(ts, indent + 2);
  ts << "{green: " << green_func_ << "}\n";
  WriteIndent(ts, indent + 2);
  ts << "{blue: " << blue_func_ << "}\n";
  WriteIndent(ts, indent + 2);
  ts << "{alpha: " << alpha_func_ << "}]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
