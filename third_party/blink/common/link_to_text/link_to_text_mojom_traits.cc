// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/link_to_text/link_to_text_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

blink::mojom::LinkGenerationError
EnumTraits<blink::mojom::LinkGenerationError,
           shared_highlighting::LinkGenerationError>::
    ToMojom(shared_highlighting::LinkGenerationError input) {
  switch (input) {
    case shared_highlighting::LinkGenerationError::kNone:
      return blink::mojom::LinkGenerationError::kNone;
    case shared_highlighting::LinkGenerationError::kIncorrectSelector:
      return blink::mojom::LinkGenerationError::kIncorrectSelector;
    case shared_highlighting::LinkGenerationError::kNoRange:
      return blink::mojom::LinkGenerationError::kNoRange;
    case shared_highlighting::LinkGenerationError::kNoContext:
      return blink::mojom::LinkGenerationError::kNoContext;
    case shared_highlighting::LinkGenerationError::kContextExhausted:
      return blink::mojom::LinkGenerationError::kContextExhausted;
    case shared_highlighting::LinkGenerationError::kContextLimitReached:
      return blink::mojom::LinkGenerationError::kContextLimitReached;
    case shared_highlighting::LinkGenerationError::kEmptySelection:
      return blink::mojom::LinkGenerationError::kEmptySelection;
    case shared_highlighting::LinkGenerationError::kTabHidden:
      return blink::mojom::LinkGenerationError::kTabHidden;
    case shared_highlighting::LinkGenerationError::kOmniboxNavigation:
      return blink::mojom::LinkGenerationError::kOmniboxNavigation;
    case shared_highlighting::LinkGenerationError::kTabCrash:
      return blink::mojom::LinkGenerationError::kTabCrash;
    case shared_highlighting::LinkGenerationError::kUnknown:
      return blink::mojom::LinkGenerationError::kUnknown;
    case shared_highlighting::LinkGenerationError::kIFrame:
      return blink::mojom::LinkGenerationError::kIFrame;
    case shared_highlighting::LinkGenerationError::kTimeout:
      return blink::mojom::LinkGenerationError::kTimeout;
    case shared_highlighting::LinkGenerationError::kBlockList:
      return blink::mojom::LinkGenerationError::kBlockList;
    case shared_highlighting::LinkGenerationError::kNoRemoteConnection:
      return blink::mojom::LinkGenerationError::kNoRemoteConnection;
    case shared_highlighting::LinkGenerationError::kNotGenerated:
      return blink::mojom::LinkGenerationError::kNotGenerated;
  }

  NOTREACHED();
}

shared_highlighting::LinkGenerationError
EnumTraits<blink::mojom::LinkGenerationError,
           shared_highlighting::LinkGenerationError>::
    FromMojom(blink::mojom::LinkGenerationError input) {
  switch (input) {
    case blink::mojom::LinkGenerationError::kNone:
      return shared_highlighting::LinkGenerationError::kNone;
    case blink::mojom::LinkGenerationError::kIncorrectSelector:
      return shared_highlighting::LinkGenerationError::kIncorrectSelector;
    case blink::mojom::LinkGenerationError::kNoRange:
      return shared_highlighting::LinkGenerationError::kNoRange;
    case blink::mojom::LinkGenerationError::kNoContext:
      return shared_highlighting::LinkGenerationError::kNoContext;
    case blink::mojom::LinkGenerationError::kContextExhausted:
      return shared_highlighting::LinkGenerationError::kContextExhausted;
    case blink::mojom::LinkGenerationError::kContextLimitReached:
      return shared_highlighting::LinkGenerationError::kContextLimitReached;
    case blink::mojom::LinkGenerationError::kEmptySelection:
      return shared_highlighting::LinkGenerationError::kEmptySelection;
    case blink::mojom::LinkGenerationError::kTabHidden:
      return shared_highlighting::LinkGenerationError::kTabHidden;
    case blink::mojom::LinkGenerationError::kOmniboxNavigation:
      return shared_highlighting::LinkGenerationError::kOmniboxNavigation;
    case blink::mojom::LinkGenerationError::kTabCrash:
      return shared_highlighting::LinkGenerationError::kTabCrash;
    case blink::mojom::LinkGenerationError::kUnknown:
      return shared_highlighting::LinkGenerationError::kUnknown;
    case blink::mojom::LinkGenerationError::kIFrame:
      return shared_highlighting::LinkGenerationError::kIFrame;
    case blink::mojom::LinkGenerationError::kTimeout:
      return shared_highlighting::LinkGenerationError::kTimeout;
    case blink::mojom::LinkGenerationError::kBlockList:
      return shared_highlighting::LinkGenerationError::kBlockList;
    case blink::mojom::LinkGenerationError::kNoRemoteConnection:
      return shared_highlighting::LinkGenerationError::kNoRemoteConnection;
    case blink::mojom::LinkGenerationError::kNotGenerated:
      return shared_highlighting::LinkGenerationError::kNotGenerated;
  }

  NOTREACHED();
}

blink::mojom::LinkGenerationReadyStatus
EnumTraits<blink::mojom::LinkGenerationReadyStatus,
           shared_highlighting::LinkGenerationReadyStatus>::
    ToMojom(shared_highlighting::LinkGenerationReadyStatus input) {
  switch (input) {
    case shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady:
      return blink::mojom::LinkGenerationReadyStatus::kRequestedBeforeReady;
    case shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady:
      return blink::mojom::LinkGenerationReadyStatus::kRequestedAfterReady;
  }

  NOTREACHED();
}

shared_highlighting::LinkGenerationReadyStatus
EnumTraits<blink::mojom::LinkGenerationReadyStatus,
           shared_highlighting::LinkGenerationReadyStatus>::
    FromMojom(blink::mojom::LinkGenerationReadyStatus input) {
  switch (input) {
    case blink::mojom::LinkGenerationReadyStatus::kRequestedBeforeReady:
      return shared_highlighting::LinkGenerationReadyStatus::
          kRequestedBeforeReady;
    case blink::mojom::LinkGenerationReadyStatus::kRequestedAfterReady:
      return shared_highlighting::LinkGenerationReadyStatus::
          kRequestedAfterReady;
  }

  NOTREACHED();
}

}  // namespace mojo
