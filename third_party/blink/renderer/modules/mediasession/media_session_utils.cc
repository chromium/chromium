// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_image.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink::media_session_utils {

HeapVector<Member<MediaImage>> ProcessArtworkVector(
    ScriptState* script_state,
    const HeapVector<Member<MediaImage>>& artwork,
    ExceptionState& exception_state) {
  HeapVector<Member<MediaImage>> processed_artwork(artwork);

  for (MediaImage* image : processed_artwork) {
    KURL url = ExecutionContext::From(script_state)->CompleteURL(image->src());
    if (!url.IsValid()) {
      exception_state.ThrowTypeError("'" + image->src() +
                                     "' can't be resolved to a valid URL.");
      return {};
    }
    image->setSrc(url);
  }

  DCHECK(!exception_state.HadException());
  return processed_artwork;
}

}  // namespace blink::media_session_utils
