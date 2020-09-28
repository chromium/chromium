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

namespace blink {

using AddressSpace = network::mojom::blink::IPAddressSpace;
using Feature = mojom::blink::WebFeature;

// Returns the kAddressSpaceLocal* WebFeature enum value corresponding to the
// given client loading a resource from the local address space, if any.
base::Optional<Feature> AddressSpaceLocalFeature(
    AddressSpace client_address_space,
    bool client_is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return client_is_secure_context
                 ? Feature::kAddressSpaceLocalEmbeddedInUnknownSecureContext
                 : Feature::kAddressSpaceLocalEmbeddedInUnknownNonSecureContext;
    case AddressSpace::kPublic:
      return client_is_secure_context
                 ? Feature::kAddressSpaceLocalEmbeddedInPublicSecureContext
                 : Feature::kAddressSpaceLocalEmbeddedInPublicNonSecureContext;
    case AddressSpace::kPrivate:
      return client_is_secure_context
                 ? Feature::kAddressSpaceLocalEmbeddedInPrivateSecureContext
                 : Feature::kAddressSpaceLocalEmbeddedInPrivateNonSecureContext;
    case AddressSpace::kLocal:
      return base::nullopt;  // Local to local is fine, we do not track it.
  }
}

// Returns the kAddressSpacePrivate* WebFeature enum value corresponding to the
// given client loading a resource from the private address space, if any.
base::Optional<Feature> AddressSpacePrivateFeature(
    AddressSpace client_address_space,
    bool client_is_secure_context) {
  switch (client_address_space) {
    case AddressSpace::kUnknown:
      return client_is_secure_context
                 ? Feature::kAddressSpacePrivateEmbeddedInUnknownSecureContext
                 : Feature::
                       kAddressSpacePrivateEmbeddedInUnknownNonSecureContext;
    case AddressSpace::kPublic:
      return client_is_secure_context
                 ? Feature::kAddressSpacePrivateEmbeddedInPublicSecureContext
                 : Feature::
                       kAddressSpacePrivateEmbeddedInPublicNonSecureContext;
    case AddressSpace::kPrivate:
    case AddressSpace::kLocal:
      // Private or local to local is fine, we do not track it.
      return base::nullopt;
  }
}

base::Optional<Feature> AddressSpaceFeature(
    AddressSpace client_address_space,
    bool client_is_secure_context,
    AddressSpace resource_address_space) {
  switch (resource_address_space) {
    case AddressSpace::kUnknown:
    case AddressSpace::kPublic:
      return base::nullopt;
    case AddressSpace::kPrivate:
      return AddressSpacePrivateFeature(client_address_space,
                                        client_is_secure_context);
    case AddressSpace::kLocal:
      return AddressSpaceLocalFeature(client_address_space,
                                      client_is_secure_context);
  }
}

}  // namespace blink
