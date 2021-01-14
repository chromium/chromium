/*
 * Copyright (C) 2020 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/address_space_feature.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-forward.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

using AddressSpace = network::mojom::blink::IPAddressSpace;
using Feature = mojom::blink::WebFeature;

// Returns the kAddressSpaceLocal* WebFeature enum value corresponding to the
// given client loading a resource from the local address space, if any.
base::Optional<Feature> AddressSpaceLocalFeatureForSubresource(
    AddressSpace client_address_space,
    bool client_is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return client_is_secure_context
                 ? Feature::kAddressSpaceUnknownSecureContextEmbeddedLocal
                 : Feature::kAddressSpaceUnknownNonSecureContextEmbeddedLocal;
    case AddressSpace::kPublic:
      return client_is_secure_context
                 ? Feature::kAddressSpacePublicSecureContextEmbeddedLocal
                 : Feature::kAddressSpacePublicNonSecureContextEmbeddedLocal;
    case AddressSpace::kPrivate:
      return client_is_secure_context
                 ? Feature::kAddressSpacePrivateSecureContextEmbeddedLocal
                 : Feature::kAddressSpacePrivateNonSecureContextEmbeddedLocal;
    case AddressSpace::kLocal:
      return base::nullopt;  // Local to local is fine, we do not track it.
  }
}

// Returns the kAddressSpacePrivate* WebFeature enum value corresponding to the
// given client loading a resource from the private address space, if any.
base::Optional<Feature> AddressSpacePrivateFeatureForSubresource(
    AddressSpace client_address_space,
    bool client_is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return client_is_secure_context
                 ? Feature::kAddressSpaceUnknownSecureContextEmbeddedPrivate
                 : Feature::kAddressSpaceUnknownNonSecureContextEmbeddedPrivate;
    case AddressSpace::kPublic:
      return client_is_secure_context
                 ? Feature::kAddressSpacePublicSecureContextEmbeddedPrivate
                 : Feature::kAddressSpacePublicNonSecureContextEmbeddedPrivate;
    case AddressSpace::kPrivate:
    case AddressSpace::kLocal:
      // Private or local to local is fine, we do not track it.
      return base::nullopt;
  }
}

base::Optional<Feature> AddressSpaceFeatureForSubresource(
    AddressSpace client_address_space,
    bool client_is_secure_context,
    AddressSpace resource_address_space) {
  switch (resource_address_space) {
    case AddressSpace::kUnknown:
    case AddressSpace::kPublic:
      return base::nullopt;
    case AddressSpace::kPrivate:
      return AddressSpacePrivateFeatureForSubresource(client_address_space,
                                                      client_is_secure_context);
    case AddressSpace::kLocal:
      return AddressSpaceLocalFeatureForSubresource(client_address_space,
                                                    client_is_secure_context);
  }
}

// Returns the kAddressSpaceLocal* WebFeature enum value corresponding to the
// given client loading a resource from the local address space, if any.
base::Optional<Feature> AddressSpaceLocalFeatureForNavigation(
    AddressSpace client_address_space,
    bool is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return is_secure_context
                 ? Feature::kAddressSpaceUnknownSecureContextNavigatedToLocal
                 : Feature::
                       kAddressSpaceUnknownNonSecureContextNavigatedToLocal;
    case AddressSpace::kPublic:
      return is_secure_context
                 ? Feature::kAddressSpacePublicSecureContextNavigatedToLocal
                 : Feature::kAddressSpacePublicNonSecureContextNavigatedToLocal;
    case AddressSpace::kPrivate:
      return is_secure_context
                 ? Feature::kAddressSpacePrivateSecureContextNavigatedToLocal
                 : Feature::
                       kAddressSpacePrivateNonSecureContextNavigatedToLocal;
    case AddressSpace::kLocal:
      return base::nullopt;  // Local to local is fine, we do not track it.
  }
}

// Returns the kAddressSpacePrivate* WebFeature enum value corresponding to the
// given client loading a resource from the private address space, if any.
base::Optional<Feature> AddressSpacePrivateFeatureForNavigation(
    AddressSpace client_address_space,
    bool is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return is_secure_context
                 ? Feature::kAddressSpaceUnknownSecureContextNavigatedToPrivate
                 : Feature::
                       kAddressSpaceUnknownNonSecureContextNavigatedToPrivate;
    case AddressSpace::kPublic:
      return is_secure_context
                 ? Feature::kAddressSpacePublicSecureContextNavigatedToPrivate
                 : Feature::
                       kAddressSpacePublicNonSecureContextNavigatedToPrivate;
    case AddressSpace::kPrivate:
    case AddressSpace::kLocal:
      // Private or local to local is fine, we do not track it.
      return base::nullopt;
  }
}

base::Optional<Feature> AddressSpaceFeatureForNavigation(
    AddressSpace client_address_space,
    bool is_secure_context,
    AddressSpace response_address_space) {
  switch (response_address_space) {
    case AddressSpace::kUnknown:
    case AddressSpace::kPublic:
      return base::nullopt;
    case AddressSpace::kPrivate:
      return AddressSpacePrivateFeatureForNavigation(client_address_space,
                                                     is_secure_context);
    case AddressSpace::kLocal:
      return AddressSpaceLocalFeatureForNavigation(client_address_space,
                                                   is_secure_context);
  }
}

base::Optional<Feature> AddressSpaceFeature(
    FetchType fetch_type,
    AddressSpace client_address_space,
    bool client_is_secure_context,
    AddressSpace response_address_space) {
  switch (fetch_type) {
    case FetchType::kSubresource:
      return AddressSpaceFeatureForSubresource(client_address_space,
                                               client_is_secure_context,
                                               response_address_space);
    case FetchType::kNavigation:
      return AddressSpaceFeatureForNavigation(client_address_space,
                                              client_is_secure_context,
                                              response_address_space);
  }
}

void RecordAddressSpaceFeature(FetchType fetch_type,
                               LocalFrame* client_frame,
                               const ResourceResponse& response) {
  if (!client_frame) {
    return;
  }

  LocalDOMWindow* window = client_frame->DomWindow();
  base::Optional<WebFeature> feature =
      AddressSpaceFeature(fetch_type, window->AddressSpace(),
                          window->IsSecureContext(), response.AddressSpace());
  if (!feature.has_value()) {
    return;
  }

  // This WebFeature encompasses all private network requests.
  UseCounter::Count(window,
                    WebFeature::kMixedContentPrivateHostnameInPublicHostname);
  UseCounter::Count(window, *feature);
}

}  // namespace blink
