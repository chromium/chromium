// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/navigation_policy_mojom_traits.h"

namespace mojo {

namespace {

using DownloadType = blink::NavigationDownloadType;
using DownloadTypes = blink::NavigationDownloadPolicy::NavigationDownloadTypes;
using DownloadTypesDataView = blink::mojom::NavigationDownloadTypesDataView;

blink::mojom::NavigationDownloadTypesPtr CreateDownloadTypes(
    const DownloadTypes& types) {
  auto data = blink::mojom::NavigationDownloadTypes::New();
  data->view_source =
      types.test(static_cast<size_t>(DownloadType::kViewSource));
  data->interstitial =
      types.test(static_cast<size_t>(DownloadType::kInterstitial));
  data->opener_cross_origin =
      types.test(static_cast<size_t>(DownloadType::kOpenerCrossOrigin));
  data->ad_frame_no_gesture =
      types.test(static_cast<size_t>(DownloadType::kAdFrameNoGesture));
  data->ad_frame =
      types.test(static_cast<size_t>(DownloadType::kAdFrame));
  data->sandbox =
      types.test(static_cast<size_t>(DownloadType::kSandbox));
  data->no_gesture =
      types.test(static_cast<size_t>(DownloadType::kNoGesture));
  return data;
}

}  // namespace

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::view_source(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kViewSource));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::interstitial(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kInterstitial));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::opener_cross_origin(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kOpenerCrossOrigin));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::ad_frame_no_gesture(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kAdFrameNoGesture));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::ad_frame(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kAdFrame));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::sandbox(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kSandbox));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::no_gesture(
    const DownloadTypes& types) {
  return types.test(static_cast<size_t>(DownloadType::kNoGesture));
}

// static
bool StructTraits<DownloadTypesDataView, DownloadTypes>::Read(
    DownloadTypesDataView in,
    DownloadTypes* out) {
  if (in.view_source())
    out->set(static_cast<size_t>(DownloadType::kViewSource));
  if (in.interstitial())
    out->set(static_cast<size_t>(DownloadType::kInterstitial));
  if (in.opener_cross_origin())
    out->set(static_cast<size_t>(DownloadType::kOpenerCrossOrigin));
  if (in.ad_frame_no_gesture())
    out->set(static_cast<size_t>(DownloadType::kAdFrameNoGesture));
  if (in.ad_frame())
    out->set(static_cast<size_t>(DownloadType::kAdFrame));
  if (in.sandbox())
    out->set(static_cast<size_t>(DownloadType::kSandbox));
  if (in.no_gesture())
    out->set(static_cast<size_t>(DownloadType::kNoGesture));
  return true;
}

// static
blink::mojom::NavigationDownloadTypesPtr
StructTraits<blink::mojom::NavigationDownloadPolicyDataView,
             blink::NavigationDownloadPolicy>::
    observed_types(const blink::NavigationDownloadPolicy& download_policy) {
  return CreateDownloadTypes(download_policy.observed_types);
}

// static
blink::mojom::NavigationDownloadTypesPtr
StructTraits<blink::mojom::NavigationDownloadPolicyDataView,
             blink::NavigationDownloadPolicy>::
    disallowed_types(const blink::NavigationDownloadPolicy& download_policy) {
  return CreateDownloadTypes(download_policy.disallowed_types);
}

// static
bool StructTraits<blink::mojom::NavigationDownloadPolicyDataView,
                  blink::NavigationDownloadPolicy>::
    Read(blink::mojom::NavigationDownloadPolicyDataView in,
         blink::NavigationDownloadPolicy* out) {
  if (!in.ReadObservedTypes(&out->observed_types) ||
      !in.ReadDisallowedTypes(&out->disallowed_types)) {
    return false;
  }
  return true;
}

}  // namespace mojo
