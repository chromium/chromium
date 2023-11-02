// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/serialized_blob_mojom_traits.h"

#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom-blink.h"

namespace mojo {

bool StructTraits<blink::mojom::blink::SerializedBlob::DataView,
                  scoped_refptr<blink::BlobDataHandle>>::
    Read(blink::mojom::blink::SerializedBlob::DataView data,
         scoped_refptr<blink::BlobDataHandle>* out) {
  WTF::String uuid;
  WTF::String type;
  if (!data.ReadUuid(&uuid) || !data.ReadContentType(&type))
    return false;
  *out = blink::BlobDataHandle::Create(
      uuid, type, data.size(),
      data.TakeBlob<mojo::PendingRemote<blink::mojom::blink::Blob>>());
  return true;
}

}  // namespace mojo
