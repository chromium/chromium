// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"

namespace blink {

ArrayBufferContents PinArrayBufferContent(
    const AllowSharedBufferSource* buffer_union) {
  ArrayBufferContents result;
  switch (buffer_union->GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferAllowShared();
      if (buffer && !buffer->IsDetached()) {
        if (buffer->IsShared()) {
          buffer->Content()->ShareWith(result);
        } else {
          static_cast<blink::DOMArrayBuffer*>(buffer)
              ->ShareNonSharedForInternalUse(result);
        }
      }
      return result;
    }
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared: {
      auto* view = buffer_union->GetAsArrayBufferViewAllowShared().Get();
      if (view && !view->IsDetached()) {
        if (view->IsShared()) {
          view->BufferShared()->Content()->ShareWith(result);
        } else {
          view->buffer()->ShareNonSharedForInternalUse(result);
        }
      }
      return result;
    }
  }
}

}  // namespace blink
