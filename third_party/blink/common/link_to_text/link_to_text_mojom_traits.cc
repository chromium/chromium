// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/link_to_text/link_to_text_mojom_traits.h"

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

  NOTREACHED_IN_MIGRATION();
  return blink::mojom::LinkGenerationError::kNone;
}

bool EnumTraits<blink::mojom::LinkGenerationError,
                shared_highlighting::LinkGenerationError>::
    FromMojom(blink::mojom::LinkGenerationError input,
              shared_highlighting::LinkGenerationError* output) {
  switch (input) {
    case blink::mojom::LinkGenerationError::kNone:
      *output = shared_highlighting::LinkGenerationError::kNone;
      return true;
    case blink::mojom::LinkGenerationError::kIncorrectSelector:
      *output = shared_highlighting::LinkGenerationError::kIncorrectSelector;
      return true;
    case blink::mojom::LinkGenerationError::kNoRange:
      *output = shared_highlighting::LinkGenerationError::kNoRange;
      return true;
    case blink::mojom::LinkGenerationError::kNoContext:
      *output = shared_highlighting::LinkGenerationError::kNoContext;
      return true;
    case blink::mojom::LinkGenerationError::kContextExhausted:
      *output = shared_highlighting::LinkGenerationError::kContextExhausted;
      return true;
    case blink::mojom::LinkGenerationError::kContextLimitReached:
      *output = shared_highlighting::LinkGenerationError::kContextLimitReached;
      return true;
    case blink::mojom::LinkGenerationError::kEmptySelection:
      *output = shared_highlighting::LinkGenerationError::kEmptySelection;
      return true;
    case blink::mojom::LinkGenerationError::kTabHidden:
      *output = shared_highlighting::LinkGenerationError::kTabHidden;
      return true;
    case blink::mojom::LinkGenerationError::kOmniboxNavigation:
      *output = shared_highlighting::LinkGenerationError::kOmniboxNavigation;
      return true;
    case blink::mojom::LinkGenerationError::kTabCrash:
      *output = shared_highlighting::LinkGenerationError::kTabCrash;
      return true;
    case blink::mojom::LinkGenerationError::kUnknown:
      *output = shared_highlighting::LinkGenerationError::kUnknown;
      return true;
    case blink::mojom::LinkGenerationError::kIFrame:
      *output = shared_highlighting::LinkGenerationError::kIFrame;
      return true;
    case blink::mojom::LinkGenerationError::kTimeout:
      *output = shared_highlighting::LinkGenerationError::kTimeout;
      return true;
    case blink::mojom::LinkGenerationError::kBlockList:
      *output = shared_highlighting::LinkGenerationError::kBlockList;
      return true;
    case blink::mojom::LinkGenerationError::kNoRemoteConnection:
      *output = shared_highlighting::LinkGenerationError::kNoRemoteConnection;
      return true;
    case blink::mojom::LinkGenerationError::kNotGenerated:
      *output = shared_highlighting::LinkGenerationError::kNotGenerated;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
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

  NOTREACHED_IN_MIGRATION();
  return blink::mojom::LinkGenerationReadyStatus::kRequestedBeforeReady;
}

bool EnumTraits<blink::mojom::LinkGenerationReadyStatus,
                shared_highlighting::LinkGenerationReadyStatus>::
    FromMojom(blink::mojom::LinkGenerationReadyStatus input,
              shared_highlighting::LinkGenerationReadyStatus* output) {
  switch (input) {
    case blink::mojom::LinkGenerationReadyStatus::kRequestedBeforeReady:
      *output =
          shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady;
      return true;
    case blink::mojom::LinkGenerationReadyStatus::kRequestedAfterReady:
      *output =
          shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
