// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist_mojom_traits.h"

namespace mojo {

namespace {

using RedirectBehavior = network::ConnectionAllowlist::RedirectBehavior;
using WebRtcBehavior = network::ConnectionAllowlist::WebRtcBehavior;

RedirectBehavior MojomToNativeRedirectBehavior(
    network::mojom::RedirectBehavior mojom_behavior) {
  switch (mojom_behavior) {
    case network::mojom::RedirectBehavior::kAllow:
      return RedirectBehavior::kAllow;
    case network::mojom::RedirectBehavior::kBlock:
      return RedirectBehavior::kBlock;
    default:
      NOTREACHED();
  }
}

WebRtcBehavior MojomToNativeWebRtcBehavior(
    network::mojom::WebRtcBehavior mojom_behavior) {
  switch (mojom_behavior) {
    case network::mojom::WebRtcBehavior::kAllow:
      return WebRtcBehavior::kAllow;
    case network::mojom::WebRtcBehavior::kBlock:
      return WebRtcBehavior::kBlock;
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
bool StructTraits<network::mojom::ConnectionAllowlistDataView,
                  network::ConnectionAllowlist>::
    Read(network::mojom::ConnectionAllowlistDataView data,
         network::ConnectionAllowlist* out) {
  if (!data.ReadAllowlist(&out->allowlist) ||
      !data.ReadReportingEndpoint(&out->reporting_endpoint) ||
      !data.ReadIssues(&out->issues)) {
    return false;
  }

  out->redirect_behavior =
      MojomToNativeRedirectBehavior(data.redirect_behavior());
  out->webrtc_behavior = MojomToNativeWebRtcBehavior(data.webrtc_behavior());

  return true;
}

// static
bool StructTraits<network::mojom::ConnectionAllowlistsDataView,
                  network::ConnectionAllowlists>::
    Read(network::mojom::ConnectionAllowlistsDataView data,
         network::ConnectionAllowlists* out) {
  if (!data.ReadEnforced(&out->enforced) ||
      !data.ReadReportOnly(&out->report_only)) {
    return false;
  }
  return true;
}

}  // namespace mojo
