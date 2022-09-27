// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/srcobject/html_media_element_src_object.h"

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_mediasourcehandle_mediastream.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_source_handle.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

namespace blink {

// static
V8MediaProvider* HTMLMediaElementSrcObject::srcObject(
    HTMLMediaElement& element) {
  HTMLMediaElement::SrcObjectVariant src_object_variant =
      element.GetSrcObjectVariant();

  if (absl::holds_alternative<blink::MediaSourceHandle*>(src_object_variant)) {
    auto* handle = absl::get<MediaSourceHandle*>(src_object_variant);
    DCHECK(handle);  // A nullptr is seen as a MediaStreamDescriptor*.
    return MakeGarbageCollected<V8MediaProvider>(
        static_cast<MediaSourceHandleImpl*>(handle));
  }

  // Otherwise, it is either null or a non-nullptr MediaStreamDescriptor*.
  auto* descriptor = absl::get<MediaStreamDescriptor*>(src_object_variant);
  if (descriptor) {
    MediaStream* stream = ToMediaStream(descriptor);
    return MakeGarbageCollected<V8MediaProvider>(stream);
  }

  return nullptr;
}

// static
void HTMLMediaElementSrcObject::setSrcObject(HTMLMediaElement& element,
                                             V8MediaProvider* media_provider) {
  if (!media_provider) {
    // Default-constructed variant is a nullptr-valued MediaStreamDescriptor*
    // since that type is the 0'th index of an
    // HTMLMediaElement::SrcObjectVariant.
    element.SetSrcObjectVariant(HTMLMediaElement::SrcObjectVariant());
    return;
  }

  switch (media_provider->GetContentType()) {
    case V8MediaProvider::ContentType::kMediaSourceHandle: {
      MediaSourceHandle* handle = media_provider->GetAsMediaSourceHandle();

      // JS null MediaProvider is a nullptr in |media_provider|, handled above.
      DCHECK(handle);

      handle->mark_used();
      element.SetSrcObjectVariant(handle);
      break;
    }
    case V8MediaProvider::ContentType::kMediaStream: {
      MediaStream* media_stream = media_provider->GetAsMediaStream();

      // JS null MediaProvider is a nullptr in |media_provider|, handled above.
      DCHECK(media_stream);

      element.SetSrcObjectVariant(media_stream->Descriptor());
      break;
    }
  }
}

}  // namespace blink
