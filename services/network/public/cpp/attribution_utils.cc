// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_utils.h"

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "services/network/public/mojom/attribution.mojom.h"

namespace network {

base::StringPiece GetAttributionSupportHeader(
    mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kWeb:
      return "web";
#if BUILDFLAG(IS_ANDROID)
    case mojom::AttributionSupport::kWebAndOs:
      return "os, web";
    case mojom::AttributionSupport::kOs:
      return "os";
    case mojom::AttributionSupport::kNone:
      return "";
#endif
  }
}

bool HasAttributionOsSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
#if BUILDFLAG(IS_ANDROID)
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
#endif
    case mojom::AttributionSupport::kWeb:
#if BUILDFLAG(IS_ANDROID)
    case mojom::AttributionSupport::kNone:
#endif
      return false;
  }
}

#if BUILDFLAG(IS_ANDROID)

bool HasAttributionWebSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kWeb:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kNone:
      return false;
  }
}

bool HasAttributionSupport(mojom::AttributionSupport attribution_support) {
  return HasAttributionWebSupport(attribution_support) ||
         HasAttributionOsSupport(attribution_support);
}

#endif

}  // namespace network
