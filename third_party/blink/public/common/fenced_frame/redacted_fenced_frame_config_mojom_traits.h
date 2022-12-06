// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom-forward.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::FencedFrameConfigDataView,
                 blink::FencedFrame::RedactedFencedFrameConfig> {
  static blink::mojom::PotentiallyOpaqueURLPtr mapped_url(
      const blink::FencedFrame::RedactedFencedFrameConfig& config);
  static blink::mojom::PotentiallyOpaqueAdAuctionDataPtr ad_auction_data(
      const blink::FencedFrame::RedactedFencedFrameConfig& config);
  static blink::mojom::PotentiallyOpaqueConfigVectorPtr nested_configs(
      const blink::FencedFrame::RedactedFencedFrameConfig& config);
  static blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
  shared_storage_budget_metadata(
      const blink::FencedFrame::RedactedFencedFrameConfig& config);
  static blink::mojom::PotentiallyOpaqueReportingMetadataPtr reporting_metadata(
      const blink::FencedFrame::RedactedFencedFrameConfig& config);

  static bool Read(blink::mojom::FencedFrameConfigDataView data,
                   blink::FencedFrame::RedactedFencedFrameConfig* out_config);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::FencedFramePropertiesDataView,
                 blink::FencedFrame::RedactedFencedFrameProperties> {
  static blink::mojom::PotentiallyOpaqueURLPtr mapped_url(
      const blink::FencedFrame::RedactedFencedFrameProperties& properties);
  static blink::mojom::PotentiallyOpaqueAdAuctionDataPtr ad_auction_data(
      const blink::FencedFrame::RedactedFencedFrameProperties& properties);
  static blink::mojom::PotentiallyOpaqueURNConfigVectorPtr
  nested_urn_config_pairs(
      const blink::FencedFrame::RedactedFencedFrameProperties& properties);
  static blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
  shared_storage_budget_metadata(
      const blink::FencedFrame::RedactedFencedFrameProperties& properties);
  static blink::mojom::PotentiallyOpaqueReportingMetadataPtr reporting_metadata(
      const blink::FencedFrame::RedactedFencedFrameProperties& properties);

  static bool Read(
      blink::mojom::FencedFramePropertiesDataView data,
      blink::FencedFrame::RedactedFencedFrameProperties* out_properties);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_MOJOM_TRAITS_H_
