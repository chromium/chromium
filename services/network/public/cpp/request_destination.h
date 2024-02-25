// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace network {

// Options for the choice of handling empty string for
// RequestDestination::kEmpty.
// See https://crbug.com/1121493 for handing empty destination
enum class EmptyRequestDestinationOption {
  kUseTheEmptyString,      // Use ""
  kUseFiveCharEmptyString  // Use "empty"
};

// Returns a string corresponding to the |dest| as defined in the spec:
// https://fetch.spec.whatwg.org/#concept-request-destination.
// When kUseFiveCharEmptyString option is used, returns "empty" instead of ""
// for RequestDestination::kEmpty.
COMPONENT_EXPORT(NETWORK_CPP)
const char* RequestDestinationToString(
    network::mojom::RequestDestination dest,
    EmptyRequestDestinationOption option =
        EmptyRequestDestinationOption::kUseTheEmptyString);

// Returns a RequestDestination corresponding to the |dest_str| as defined in
// the spec: https://fetch.spec.whatwg.org/#concept-request-destination. If
// |dest_str| is not a valid RequestDestination, returns nullopt.
// When kUseFiveCharEmptyString option is used, "empty" is accepted for
// RequestDestination::kEmpty. But the empty string "" is not accepted.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<network::mojom::RequestDestination> RequestDestinationFromString(
    std::string_view dest_str,
    EmptyRequestDestinationOption option =
        EmptyRequestDestinationOption::kUseTheEmptyString);

// Returns a string representation of the `destination` for histogram recording.
// This just calls  RequestDestinationToString() with kUseFiveCharEmptyString
// option.
COMPONENT_EXPORT(NETWORK_CPP)
const char* RequestDestinationToStringForHistogram(
    network::mojom::RequestDestination dest);

// Returns whether the destination is a frame embedded in the document.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsRequestDestinationEmbeddedFrame(network::mojom::RequestDestination dest);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_
