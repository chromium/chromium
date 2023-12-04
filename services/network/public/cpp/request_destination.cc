// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/request_destination.h"

namespace network {

// These strings are used in histograms, so do not change the values without
// updating/deprecating histograms which use RequestDestination.

const char* RequestDestinationToString(
    network::mojom::RequestDestination dest) {
  switch (dest) {
    case network::mojom::RequestDestination::kEmpty:
      // See https://crbug.com/1121493
      return "";
    case network::mojom::RequestDestination::kAudio:
      return "audio";
    case network::mojom::RequestDestination::kAudioWorklet:
      return "audioworklet";
    case network::mojom::RequestDestination::kDocument:
      return "document";
    case network::mojom::RequestDestination::kEmbed:
      return "embed";
    case network::mojom::RequestDestination::kFont:
      return "font";
    case network::mojom::RequestDestination::kFrame:
      return "frame";
    case network::mojom::RequestDestination::kIframe:
      return "iframe";
    case network::mojom::RequestDestination::kImage:
      return "image";
    case network::mojom::RequestDestination::kManifest:
      return "manifest";
    case network::mojom::RequestDestination::kObject:
      return "object";
    case network::mojom::RequestDestination::kPaintWorklet:
      return "paintworklet";
    case network::mojom::RequestDestination::kReport:
      return "report";
    case network::mojom::RequestDestination::kScript:
      return "script";
    case network::mojom::RequestDestination::kServiceWorker:
      return "serviceworker";
    case network::mojom::RequestDestination::kSharedWorker:
      return "sharedworker";
    case network::mojom::RequestDestination::kStyle:
      return "style";
    case network::mojom::RequestDestination::kTrack:
      return "track";
    case network::mojom::RequestDestination::kVideo:
      return "video";
    case network::mojom::RequestDestination::kWebBundle:
      return "webbundle";
    case network::mojom::RequestDestination::kWorker:
      return "worker";
    case network::mojom::RequestDestination::kXslt:
      return "xslt";
    case network::mojom::RequestDestination::kFencedframe:
      return "fencedframe";
    case network::mojom::RequestDestination::kWebIdentity:
      return "webidentity";
    case network::mojom::RequestDestination::kDictionary:
      return "dictionary";
    case network::mojom::RequestDestination::kSpeculationRules:
      return "speculationrules";
  }
}

const char* RequestDestinationToStringForHistogram(
    network::mojom::RequestDestination dest) {
  return dest == network::mojom::RequestDestination::kEmpty
             ? "empty"
             : RequestDestinationToString(dest);
}

bool IsRequestDestinationEmbeddedFrame(
    network::mojom::RequestDestination dest) {
  return dest == network::mojom::RequestDestination::kFrame ||
         dest == network::mojom::RequestDestination::kIframe ||
         dest == network::mojom::RequestDestination::kFencedframe;
}

}  // namespace network
