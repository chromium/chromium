/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_mixed_content.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"

namespace blink {

// static
WebMixedContentContextType WebMixedContent::ContextTypeFromRequestContext(
    mojom::blink::RequestContextType context,
    WebMixedContent::CheckModeForPlugin check_mode_for_plugin) {
  switch (context) {
    // "Optionally-blockable" mixed content
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::IMAGE:
    case mojom::blink::RequestContextType::VIDEO:
      return WebMixedContentContextType::kOptionallyBlockable;

    // Plugins! Oh how dearly we love plugin-loaded content!
    case mojom::blink::RequestContextType::PLUGIN: {
      return check_mode_for_plugin ==
                     WebMixedContent::CheckModeForPlugin::kStrict
                 ? WebMixedContentContextType::kBlockable
                 : WebMixedContentContextType::kOptionallyBlockable;
    }

    // "Blockable" mixed content
    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::CSP_REPORT:
    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FAVICON:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::FONT:
    case mojom::blink::RequestContextType::FORM:
    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::IFRAME:
    case mojom::blink::RequestContextType::IMAGE_SET:
    case mojom::blink::RequestContextType::IMPORT:
    case mojom::blink::RequestContextType::INTERNAL:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::MANIFEST:
    case mojom::blink::RequestContextType::OBJECT:
    case mojom::blink::RequestContextType::PING:
    case mojom::blink::RequestContextType::PREFETCH:
    case mojom::blink::RequestContextType::SCRIPT:
    case mojom::blink::RequestContextType::SERVICE_WORKER:
    case mojom::blink::RequestContextType::SHARED_WORKER:
    case mojom::blink::RequestContextType::STYLE:
    case mojom::blink::RequestContextType::SUBRESOURCE:
    case mojom::blink::RequestContextType::TRACK:
    case mojom::blink::RequestContextType::WORKER:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
    case mojom::blink::RequestContextType::XSLT:
      return WebMixedContentContextType::kBlockable;

    // FIXME: Contexts that we should block, but don't currently.
    // https://crbug.com/388650
    case mojom::blink::RequestContextType::DOWNLOAD:
      return WebMixedContentContextType::kShouldBeBlockable;

    case mojom::blink::RequestContextType::UNSPECIFIED:
      NOTREACHED();
  }
  NOTREACHED();
  return WebMixedContentContextType::kBlockable;
}

}  // namespace blink
