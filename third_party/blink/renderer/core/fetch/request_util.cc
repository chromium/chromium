// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request_util.h"

#include "services/network/public/mojom/fetch_api.mojom-blink.h"

namespace blink {

network::mojom::RequestMode V8RequestModeToMojom(const V8RequestMode& mode) {
  switch (mode.AsEnum()) {
    case blink::V8RequestMode::Enum::kSameOrigin:
      return network::mojom::RequestMode::kSameOrigin;
    case blink::V8RequestMode::Enum::kNoCors:
      return network::mojom::RequestMode::kNoCors;
    case blink::V8RequestMode::Enum::kCors:
      return network::mojom::RequestMode::kCors;
    case blink::V8RequestMode::Enum::kNavigate:
      return network::mojom::RequestMode::kNavigate;
  }
}

network::mojom::RequestDestination V8RequestDestinationToMojom(
    const V8RequestDestination& destination) {
  switch (destination.AsEnum()) {
    case blink::V8RequestDestination::Enum::k:
      return network::mojom::RequestDestination::kEmpty;
    case blink::V8RequestDestination::Enum::kAudio:
      return network::mojom::RequestDestination::kAudio;
    case blink::V8RequestDestination::Enum::kAudioworklet:
      return network::mojom::RequestDestination::kAudioWorklet;
    case blink::V8RequestDestination::Enum::kDocument:
      return network::mojom::RequestDestination::kDocument;
    case blink::V8RequestDestination::Enum::kEmbed:
      return network::mojom::RequestDestination::kEmbed;
    case blink::V8RequestDestination::Enum::kFont:
      return network::mojom::RequestDestination::kFont;
    case blink::V8RequestDestination::Enum::kFrame:
      return network::mojom::RequestDestination::kFrame;
    case blink::V8RequestDestination::Enum::kIFrame:
      return network::mojom::RequestDestination::kIframe;
    case blink::V8RequestDestination::Enum::kImage:
      return network::mojom::RequestDestination::kImage;
    case blink::V8RequestDestination::Enum::kJson:
      return network::mojom::RequestDestination::kJson;
    case blink::V8RequestDestination::Enum::kManifest:
      return network::mojom::RequestDestination::kManifest;
    case blink::V8RequestDestination::Enum::kObject:
      return network::mojom::RequestDestination::kObject;
    case blink::V8RequestDestination::Enum::kPaintworklet:
      return network::mojom::RequestDestination::kPaintWorklet;
    case blink::V8RequestDestination::Enum::kReport:
      return network::mojom::RequestDestination::kReport;
    case blink::V8RequestDestination::Enum::kScript:
      return network::mojom::RequestDestination::kScript;
    case blink::V8RequestDestination::Enum::kSharedworker:
      return network::mojom::RequestDestination::kSharedWorker;
    case blink::V8RequestDestination::Enum::kStyle:
      return network::mojom::RequestDestination::kStyle;
    case blink::V8RequestDestination::Enum::kTrack:
      return network::mojom::RequestDestination::kTrack;
    case blink::V8RequestDestination::Enum::kVideo:
      return network::mojom::RequestDestination::kVideo;
    case blink::V8RequestDestination::Enum::kWorker:
      return network::mojom::RequestDestination::kWorker;
    case blink::V8RequestDestination::Enum::kXslt:
      return network::mojom::RequestDestination::kXslt;
    case blink::V8RequestDestination::Enum::kFencedframe:
      return network::mojom::RequestDestination::kFencedframe;
    case blink::V8RequestDestination::Enum::kDictionary:
      return network::mojom::RequestDestination::kDictionary;
    case blink::V8RequestDestination::Enum::kSpeculationrules:
      return network::mojom::RequestDestination::kSpeculationRules;
  }
}

}  // namespace blink
