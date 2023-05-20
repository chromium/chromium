// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/link_dictionary_resource.h"

#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool CompressionDictionaryTransportFullyEnabled(
    const FeatureContext* feature_context) {
  return RuntimeEnabledFeatures::CompressionDictionaryTransportEnabled(
             feature_context) &&
         RuntimeEnabledFeatures::CompressionDictionaryTransportBackendEnabled();
}

Resource* LinkDictionaryResource::Fetch(FetchParameters& params,
                                        ResourceFetcher* fetcher) {
  return fetcher->RequestResource(params, Factory(), nullptr);
}

LinkDictionaryResource::LinkDictionaryResource(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options)
    : Resource(request, ResourceType::kDictionary, options) {}

LinkDictionaryResource::~LinkDictionaryResource() = default;

LinkDictionaryResource::Factory::Factory()
    : NonTextResourceFactory(ResourceType::kDictionary) {}

Resource* LinkDictionaryResource::Factory::Create(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options) const {
  return MakeGarbageCollected<LinkDictionaryResource>(request, options);
}

}  // namespace blink
