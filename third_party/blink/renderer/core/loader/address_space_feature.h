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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_

#include "base/optional.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Returns the kAddressSpace* WebFeature enum value corresponding to a client
// in |client_address_space| loading a resource from |resource_address_space|,
// if any.
//
// |client_is_secure_context| specifies whether the client execution context is
// a secure context, as defined in
// https://html.spec.whatwg.org/multipage/webappapis.html#secure-context.
//
// Returns nullopt if the load is not a private network request, as defined in
// https://wicg.github.io/cors-rfc1918/#private-network-request.
base::Optional<mojom::blink::WebFeature> CORE_EXPORT AddressSpaceFeature(
    network::mojom::blink::IPAddressSpace client_address_space,
    bool client_is_secure_context,
    network::mojom::blink::IPAddressSpace resource_address_space);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_
