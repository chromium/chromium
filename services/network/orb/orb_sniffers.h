// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORB_ORB_SNIFFERS_H_
#define SERVICES_NETWORK_ORB_ORB_SNIFFERS_H_

#include <string_view>

#include "base/component_export.h"

// This header provides an implementation for "sniffers" used by ORB.
// The implementation should typically be used through the public orb_api.h.
namespace network::orb {

// Three conclusions are possible from sniffing a byte sequence:
//  - No: meaning that the data definitively doesn't match the indicated type.
//  - Yes: meaning that the data definitive does match the indicated type.
//  - Maybe: meaning that if more bytes are appended to the stream, it's
//    possible to get a Yes result. For example, if we are sniffing for a tag
//    like "<html", a kMaybe result would occur if the data contains just
//    "<ht".
enum SniffingResult {
  kNo,
  kMaybe,
  kYes,
};

COMPONENT_EXPORT(NETWORK_SERVICE)
SniffingResult SniffForHTML(std::string_view data);
COMPONENT_EXPORT(NETWORK_SERVICE)
SniffingResult SniffForXML(std::string_view data);
COMPONENT_EXPORT(NETWORK_SERVICE)
SniffingResult SniffForJSON(std::string_view data);

// Sniff for patterns that indicate |data| only ought to be consumed by XHR()
// or fetch(). This detects Javascript parser-breaker and particular JS
// infinite-loop patterns, which are used conventionally as a defense against
// JSON data exfiltration by means of a <script> tag.
COMPONENT_EXPORT(NETWORK_SERVICE)
SniffingResult SniffForFetchOnlyResource(std::string_view data);

}  // namespace network::orb

#endif  // SERVICES_NETWORK_ORB_ORB_SNIFFERS_H_
