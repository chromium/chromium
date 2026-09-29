/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

#include <utility>

#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

ResourceLoaderOptions::ResourceLoaderOptions(
    const DOMWrapperWorld* target_world,
    const DOMWrapperWorld* world_for_csp)
    : data_buffering_policy(kBufferData),
      content_security_policy_option(network::mojom::CSPDisposition::CHECK),
      synchronous_policy(kRequestAsynchronously),
      parser_disposition(kParserInserted),
      cache_aware_loading_enabled(kNotCacheAwareLoadingEnabled),
      target_world_(target_world),
      world_for_csp_(world_for_csp) {
  if (target_world_) {
    // Isolated World Resource.
    CHECK(target_world_->IsIsolatedWorld());
    CHECK_EQ(target_world_, world_for_csp_);
  } else if (world_for_csp_) {
    // Main World Resource with isolated world CSP.
    CHECK(world_for_csp_->IsIsolatedWorld());
  }
}

ResourceLoaderOptions::ResourceLoaderOptions(
    const DOMWrapperWorld* world_for_csp)
    : ResourceLoaderOptions(/*target_world=*/nullptr,
                            world_for_csp && world_for_csp->IsIsolatedWorld()
                                ? world_for_csp
                                : nullptr) {}

ResourceLoaderOptions ResourceLoaderOptions::CreateForTargetWorld(
    const DOMWrapperWorld* target_world) {
  if (target_world && target_world->IsIsolatedWorld()) {
    // Isolated World Resource.
    return ResourceLoaderOptions(/*target_world=*/target_world,
                                 /*world_for_csp=*/target_world);
  }
  // Main World Resource.
  return ResourceLoaderOptions(/*target_world=*/nullptr,
                               /*world_for_csp=*/nullptr);
}

ResourceLoaderOptions::ResourceLoaderOptions(
    const ResourceLoaderOptions& other) = default;

ResourceLoaderOptions& ResourceLoaderOptions::operator=(
    const ResourceLoaderOptions& other) = default;

ResourceLoaderOptions::ResourceLoaderOptions(ResourceLoaderOptions&& other) =
    default;

ResourceLoaderOptions& ResourceLoaderOptions::operator=(
    ResourceLoaderOptions&& other) = default;

ResourceLoaderOptions::~ResourceLoaderOptions() = default;

}  // namespace blink
