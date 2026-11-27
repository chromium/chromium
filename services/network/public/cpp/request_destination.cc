// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/request_destination.h"

#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"

namespace network {

namespace {

// These strings are used in histograms, so do not change the values without
// updating/deprecating histograms which use RequestDestination.
//
// When updating this, consider also updating RequestDestination in
// third_party/blink/renderer/core/fetch/request.idl.

// LINT.IfChange
constexpr char kEmpty[] = "";
constexpr char kAudio[] = "audio";
constexpr char kAudioWorklet[] = "audioworklet";
constexpr char kDocument[] = "document";
constexpr char kEmbed[] = "embed";
constexpr char kFont[] = "font";
constexpr char kFrame[] = "frame";
constexpr char kIframe[] = "iframe";
constexpr char kImage[] = "image";
constexpr char kJson[] = "json";
constexpr char kManifest[] = "manifest";
constexpr char kObject[] = "object";
constexpr char kPaintWorklet[] = "paintworklet";
constexpr char kReport[] = "report";
constexpr char kScript[] = "script";
constexpr char kServiceWorker[] = "serviceworker";
constexpr char kSharedWorker[] = "sharedworker";
constexpr char kStyle[] = "style";
constexpr char kTrack[] = "track";
constexpr char kVideo[] = "video";
constexpr char kWebBundle[] = "webbundle";
constexpr char kWorker[] = "worker";
constexpr char kXslt[] = "xslt";
constexpr char kFencedframe[] = "fencedframe";
constexpr char kWebIdentity[] = "webidentity";
constexpr char kEmailVerification[] = "email-verification";
constexpr char kCompressionDictionary[] = "compression-dictionary";
constexpr char kSpeculationRules[] = "speculationrules";
constexpr char kText[] = "text";

constexpr auto kRequestDestinationToStringMap =
    base::MakeFixedFlatMap<network::mojom::RequestDestination, const char*>(
        {{network::mojom::RequestDestination::kEmpty, kEmpty},
         {network::mojom::RequestDestination::kAudio, kAudio},
         {network::mojom::RequestDestination::kAudioWorklet, kAudioWorklet},
         {network::mojom::RequestDestination::kDocument, kDocument},
         {network::mojom::RequestDestination::kEmbed, kEmbed},
         {network::mojom::RequestDestination::kFont, kFont},
         {network::mojom::RequestDestination::kFrame, kFrame},
         {network::mojom::RequestDestination::kIframe, kIframe},
         {network::mojom::RequestDestination::kImage, kImage},
         {network::mojom::RequestDestination::kManifest, kManifest},
         {network::mojom::RequestDestination::kObject, kObject},
         {network::mojom::RequestDestination::kPaintWorklet, kPaintWorklet},
         {network::mojom::RequestDestination::kReport, kReport},
         {network::mojom::RequestDestination::kScript, kScript},
         {network::mojom::RequestDestination::kServiceWorker, kServiceWorker},
         {network::mojom::RequestDestination::kSharedWorker, kSharedWorker},
         {network::mojom::RequestDestination::kStyle, kStyle},
         {network::mojom::RequestDestination::kText, kText},
         {network::mojom::RequestDestination::kTrack, kTrack},
         {network::mojom::RequestDestination::kVideo, kVideo},
         {network::mojom::RequestDestination::kWebBundle, kWebBundle},
         {network::mojom::RequestDestination::kWorker, kWorker},
         {network::mojom::RequestDestination::kXslt, kXslt},
         {network::mojom::RequestDestination::kFencedframe, kFencedframe},
         {network::mojom::RequestDestination::kWebIdentity, kWebIdentity},
         {network::mojom::RequestDestination::kEmailVerification,
          kEmailVerification},
         {network::mojom::RequestDestination::kCompressionDictionary,
          kCompressionDictionary},
         {network::mojom::RequestDestination::kSpeculationRules,
          kSpeculationRules},
         {network::mojom::RequestDestination::kJson, kJson}});
// LINT.ThenChange(/third_party/blink/renderer/core/fetch/request.idl)

constexpr auto kRequestDestinationFromStringMap =
    base::MakeFixedFlatMap<std::string_view,
                           network::mojom::RequestDestination>(
        kRequestDestinationToStringMap,
        /*comp=*/{},
        [](const auto& entry) { return std::pair(entry.second, entry.first); });

// Gaps in the network::mojom::RequestDestination enum due to deprecated types.
// When a type is deprecated and removed from the mojom definition, its numeric
// value is not reused. This introduces gaps in the enum values.
// These must be kept in sync with the mojom definition to ensure the static
// asserts below work correctly.
constexpr int kDeprecatedRequestDestinationValues[] = {
    27,  // kSharedStorageWorklet
};

constexpr size_t CountDeprecatedRequestDestinationValues(int max_value) {
  size_t count = 0;
  for (int value : kDeprecatedRequestDestinationValues) {
    if (value < max_value) {
      count++;
    }
  }
  return count;
}

static_assert(
    std::size(kRequestDestinationToStringMap) +
            CountDeprecatedRequestDestinationValues(static_cast<int>(
                network::mojom::RequestDestination::kMaxValue)) ==
        static_cast<size_t>(network::mojom::RequestDestination::kMaxValue) + 1,
    "All non-deprecated types must be in kRequestDestinationToStringMap.");

constexpr char kFiveCharEmptyString[] = "empty";

}  // namespace

const char* RequestDestinationToString(network::mojom::RequestDestination dest,
                                       EmptyRequestDestinationOption option) {
  if (option == EmptyRequestDestinationOption::kUseFiveCharEmptyString &&
      dest == network::mojom::RequestDestination::kEmpty) {
    return kFiveCharEmptyString;
  }
  return kRequestDestinationToStringMap.at(dest);
}

std::optional<network::mojom::RequestDestination> RequestDestinationFromString(
    std::string_view dest_str,
    EmptyRequestDestinationOption option) {
  if (option == EmptyRequestDestinationOption::kUseFiveCharEmptyString) {
    if (dest_str == kFiveCharEmptyString) {
      return network::mojom::RequestDestination::kEmpty;
    }
    if (dest_str.empty()) {
      return std::nullopt;
    }
  }
  auto it = kRequestDestinationFromStringMap.find(dest_str);
  if (it == kRequestDestinationFromStringMap.end()) {
    return std::nullopt;
  }
  return it->second;
}

const char* RequestDestinationToStringForHistogram(
    network::mojom::RequestDestination dest) {
  return RequestDestinationToString(
      dest, EmptyRequestDestinationOption::kUseFiveCharEmptyString);
}

bool IsRequestDestinationEmbeddedFrame(
    network::mojom::RequestDestination dest) {
  return dest == network::mojom::RequestDestination::kFrame ||
         dest == network::mojom::RequestDestination::kIframe ||
         dest == network::mojom::RequestDestination::kFencedframe;
}

}  // namespace network
