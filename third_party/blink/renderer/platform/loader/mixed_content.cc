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

#include "third_party/blink/renderer/platform/loader/mixed_content.h"

#include "base/notreached.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-blink.h"

namespace blink {

// static
mojom::blink::MixedContentContextType
MixedContent::ContextTypeFromRequestContext(
    mojom::blink::RequestContextType context,
    MixedContent::CheckModeForPlugin check_mode_for_plugin) {
  switch (context) {
    // "Optionally-blockable" mixed content
    case mojom::RequestContextType::AUDIO:
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::VIDEO:
      return mojom::blink::MixedContentContextType::kOptionallyBlockable;

    // Plugins! Oh how dearly we love plugin-loaded content!
    case mojom::RequestContextType::PLUGIN: {
      return check_mode_for_plugin == MixedContent::CheckModeForPlugin::kStrict
                 ? mojom::blink::MixedContentContextType::kBlockable
                 : mojom::blink::MixedContentContextType::kOptionallyBlockable;
    }

    // "Blockable" mixed content
    case mojom::RequestContextType::ATTRIBUTION_SRC:
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::CSP_REPORT:
    case mojom::RequestContextType::EMBED:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::FONT:
    case mojom::RequestContextType::FORM:
    case mojom::RequestContextType::FRAME:
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::IFRAME:
    case mojom::RequestContextType::IMAGE_SET:
    case mojom::RequestContextType::INTERNAL:
    case mojom::RequestContextType::JSON:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::MANIFEST:
    case mojom::RequestContextType::OBJECT:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::PREFETCH:
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::SERVICE_WORKER:
    case mojom::RequestContextType::SHARED_WORKER:
    case mojom::RequestContextType::SPECULATION_RULES:
    case mojom::RequestContextType::STYLE:
    case mojom::RequestContextType::SUBRESOURCE:
    case mojom::RequestContextType::SUBRESOURCE_WEBBUNDLE:
    case mojom::RequestContextType::TRACK:
    case mojom::RequestContextType::WORKER:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::XSLT:
      return mojom::blink::MixedContentContextType::kBlockable;

    // FIXME: Contexts that we should block, but don't currently.
    // https://crbug.com/388650
    case mojom::RequestContextType::DOWNLOAD:
      return mojom::blink::MixedContentContextType::kShouldBeBlockable;

    case mojom::RequestContextType::UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::blink::MixedContentContextType::kBlockable;
}

}  // namespace blink
