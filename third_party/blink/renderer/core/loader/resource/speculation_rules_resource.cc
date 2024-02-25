// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/speculation_rules_resource.h"

#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

SpeculationRulesResource* SpeculationRulesResource::Fetch(
    FetchParameters& params,
    ResourceFetcher* fetcher) {
  return To<SpeculationRulesResource>(
      fetcher->RequestResource(params, Factory(), nullptr));
}

SpeculationRulesResource::SpeculationRulesResource(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options)
    : TextResource(request,
                   ResourceType::kSpeculationRules,
                   options,
                   TextResourceDecoderOptions::CreateUTF8Decode()) {}

SpeculationRulesResource::~SpeculationRulesResource() = default;

SpeculationRulesResource::Factory::Factory()
    : ResourceFactory(ResourceType::kSpeculationRules,
                      TextResourceDecoderOptions::kPlainTextContent) {}

Resource* SpeculationRulesResource::Factory::Create(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& /*decoder_options*/) const {
  // Ignore decoder options and always use UTF-8 decoding.
  return MakeGarbageCollected<SpeculationRulesResource>(request, options);
}

}  // namespace blink
