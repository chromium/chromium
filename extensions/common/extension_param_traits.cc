// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace IPC {

void ParamTraits<extensions::mojom::ExtraResponseDataPtr>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, !p.is_null());
  if (!p)
    return;
  m->WriteUInt32(p->blobs.size());
  for (const auto& blob : p->blobs) {
    m->WriteString(blob->uuid);
    m->WriteString(blob->content_type);
    m->WriteUInt64(blob->size);
    MojoMessageHelper::WriteMessagePipeTo(m, blob->blob.PassPipe());
  }
}

bool ParamTraits<extensions::mojom::ExtraResponseDataPtr>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool is_not_null;
  if (!ReadParam(m, iter, &is_not_null))
    return false;
  if (!is_not_null)
    return true;

  *r = extensions::mojom::ExtraResponseData::New();

  uint32_t blob_count;
  if (!ReadParam(m, iter, &blob_count))
    return false;

  (*r)->blobs.resize(blob_count);

  for (auto& blob : (*r)->blobs) {
    blob = blink::mojom::SerializedBlob::New();
    if (!ReadParam(m, iter, &blob->uuid) ||
        !ReadParam(m, iter, &blob->content_type) ||
        !ReadParam(m, iter, &blob->size)) {
      return false;
    }

    mojo::ScopedMessagePipeHandle blob_handle;
    if (!MojoMessageHelper::ReadMessagePipeFrom(m, iter, &blob_handle) ||
        !blob_handle.is_valid())
      return false;
    blob->blob = mojo::PendingRemote<blink::mojom::Blob>(
        std::move(blob_handle), blink::mojom::Blob::Version_);
  }

  return true;
}

void ParamTraits<extensions::mojom::ExtraResponseDataPtr>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<extensions::ExtraResponseData>");
}

}  // namespace IPC
