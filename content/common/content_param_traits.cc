// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_param_traits.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

namespace IPC {

void ParamTraits<blink::MessagePortChannel>::Write(base::Pickle* m,
                                                   const param_type& p) {
  ParamTraits<blink::MessagePortDescriptor>::Write(m, p.ReleaseHandle());
}

bool ParamTraits<blink::MessagePortChannel>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* r) {
  blink::MessagePortDescriptor port;
  if (!ParamTraits<blink::MessagePortDescriptor>::Read(m, iter, &port))
    return false;
  *r = blink::MessagePortChannel(std::move(port));
  return true;
}

void ParamTraits<blink::MessagePortChannel>::Log(const param_type& p,
                                                 std::string* l) {}

void ParamTraits<blink::PolicyValue>::Write(base::Pickle* m,
                                            const param_type& p) {
  blink::mojom::PolicyValueType type = p.Type();
  WriteParam(m, static_cast<int>(type));
  switch (type) {
    case blink::mojom::PolicyValueType::kBool:
      WriteParam(m, p.BoolValue());
      break;
    case blink::mojom::PolicyValueType::kDecDouble:
      WriteParam(m, p.DoubleValue());
      break;
    case blink::mojom::PolicyValueType::kEnum:
      WriteParam(m, p.IntValue());
      break;
    case blink::mojom::PolicyValueType::kNull:
      break;
  }
}

void ParamTraits<blink::MessagePortDescriptor>::Write(
    base::Pickle* m,
    const param_type& pconst) {
  // Serializing this object is a move of the object contents, thus we need a
  // mutable reference to it.
  param_type& p = const_cast<param_type&>(pconst);
  ParamTraits<mojo::MessagePipeHandle>::Write(
      m, p.TakeHandleForSerialization().release());
  ParamTraits<base::UnguessableToken>::Write(m, p.TakeIdForSerialization());
  ParamTraits<uint64_t>::Write(m, p.TakeSequenceNumberForSerialization());
}

bool ParamTraits<blink::MessagePortDescriptor>::Read(const base::Pickle* m,
                                                     base::PickleIterator* iter,
                                                     param_type* r) {
  mojo::MessagePipeHandle port;
  base::UnguessableToken id;
  uint64_t sequence_number = 0;
  if (!ParamTraits<mojo::MessagePipeHandle>::Read(m, iter, &port) ||
      !ParamTraits<base::UnguessableToken>::Read(m, iter, &id) ||
      !ParamTraits<uint64_t>::Read(m, iter, &sequence_number)) {
    return false;
  }
  r->InitializeFromSerializedValues(mojo::ScopedMessagePipeHandle(port), id,
                                    sequence_number);
  return true;
}

void ParamTraits<blink::MessagePortDescriptor>::Log(const param_type& p,
                                                    std::string* l) {}

bool ParamTraits<blink::PolicyValue>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
  int int_type;
  if (!ReadParam(m, iter, &int_type))
    return false;
  blink::mojom::PolicyValueType type =
      static_cast<blink::mojom::PolicyValueType>(int_type);
  r->SetType(type);
  switch (type) {
    case blink::mojom::PolicyValueType::kBool: {
      bool b;
      if (!ReadParam(m, iter, &b))
        return false;
      r->SetBoolValue(b);
      break;
    }
    case blink::mojom::PolicyValueType::kDecDouble: {
      double d;
      if (!ReadParam(m, iter, &d))
        return false;
      r->SetDoubleValue(d);
      break;
    }
    case blink::mojom::PolicyValueType::kEnum: {
      int32_t i;
      if (!ReadParam(m, iter, &i))
        return false;
      r->SetIntValue(i);
      break;
    }
    case blink::mojom::PolicyValueType::kNull:
      break;
  }
  return true;
}

void ParamTraits<blink::PolicyValue>::Log(const param_type& p, std::string* l) {
}

void ParamTraits<ui::AXMode>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.flags());
}

bool ParamTraits<ui::AXMode>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* r) {
  uint32_t value;
  if (!ReadParam(m, iter, &value))
    return false;
  *r = ui::AXMode(value);
  return true;
}

void ParamTraits<ui::AXMode>::Log(const param_type& p, std::string* l) {}

template <>
struct ParamTraits<blink::mojom::SerializedBlobPtr> {
  using param_type = blink::mojom::SerializedBlobPtr;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p->uuid);
    WriteParam(m, p->content_type);
    WriteParam(m, p->size);
    WriteParam(m, p->blob.PassPipe().release());
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    *r = blink::mojom::SerializedBlob::New();
    mojo::MessagePipeHandle handle;
    if (!ReadParam(m, iter, &(*r)->uuid) ||
        !ReadParam(m, iter, &(*r)->content_type) ||
        !ReadParam(m, iter, &(*r)->size) || !ReadParam(m, iter, &handle)) {
      return false;
    }
    (*r)->blob = mojo::PendingRemote<blink::mojom::Blob>(
        mojo::ScopedMessagePipeHandle(handle), blink::mojom::Blob::Version_);
    return true;
  }
};

template <>
struct ParamTraits<
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>> {
  using param_type =
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>;
  static void Write(base::Pickle* m, const param_type& p) {
    // Move the Mojo pipe to serialize the
    // PendingRemote<FileSystemAccessTransferToken> for a postMessage() target.
    WriteParam(m, const_cast<param_type&>(p).PassPipe().release());
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    mojo::MessagePipeHandle handle;
    if (!ReadParam(m, iter, &handle)) {
      return false;
    }
    *r = mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>(
        mojo::ScopedMessagePipeHandle(handle),
        blink::mojom::FileSystemAccessTransferToken::Version_);
    return true;
  }
};

void ParamTraits<viz::FrameSinkId>::Write(base::Pickle* m,
                                          const param_type& p) {
  DCHECK(p.is_valid());
  WriteParam(m, p.client_id());
  WriteParam(m, p.sink_id());
}

bool ParamTraits<viz::FrameSinkId>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  uint32_t client_id;
  if (!ReadParam(m, iter, &client_id))
    return false;

  uint32_t sink_id;
  if (!ReadParam(m, iter, &sink_id))
    return false;

  *p = viz::FrameSinkId(client_id, sink_id);
  return p->is_valid();
}

void ParamTraits<viz::FrameSinkId>::Log(const param_type& p, std::string* l) {
  l->append("viz::FrameSinkId(");
  LogParam(p.client_id(), l);
  l->append(", ");
  LogParam(p.sink_id(), l);
  l->append(")");
}

void ParamTraits<viz::LocalSurfaceId>::Write(base::Pickle* m,
                                             const param_type& p) {
  DCHECK(p.is_valid());
  WriteParam(m, p.parent_sequence_number());
  WriteParam(m, p.child_sequence_number());
  WriteParam(m, p.embed_token());
}

bool ParamTraits<viz::LocalSurfaceId>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  uint32_t parent_sequence_number;
  if (!ReadParam(m, iter, &parent_sequence_number))
    return false;

  uint32_t child_sequence_number;
  if (!ReadParam(m, iter, &child_sequence_number))
    return false;

  base::UnguessableToken embed_token;
  if (!ReadParam(m, iter, &embed_token))
    return false;

  *p = viz::LocalSurfaceId(parent_sequence_number, child_sequence_number,
                           embed_token);
  return p->is_valid();
}

void ParamTraits<viz::LocalSurfaceId>::Log(const param_type& p,
                                           std::string* l) {
  l->append("viz::LocalSurfaceId(");
  LogParam(p.parent_sequence_number(), l);
  l->append(", ");
  LogParam(p.child_sequence_number(), l);
  l->append(", ");
  LogParam(p.embed_token(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceId>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.frame_sink_id());
  WriteParam(m, p.local_surface_id());
}

bool ParamTraits<viz::SurfaceId>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  viz::FrameSinkId frame_sink_id;
  if (!ReadParam(m, iter, &frame_sink_id))
    return false;

  viz::LocalSurfaceId local_surface_id;
  if (!ReadParam(m, iter, &local_surface_id))
    return false;

  *p = viz::SurfaceId(frame_sink_id, local_surface_id);
  return true;
}

void ParamTraits<viz::SurfaceId>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceId(");
  LogParam(p.frame_sink_id(), l);
  l->append(", ");
  LogParam(p.local_surface_id(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceInfo>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.id());
  WriteParam(m, p.device_scale_factor());
  WriteParam(m, p.size_in_pixels());
}

bool ParamTraits<viz::SurfaceInfo>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  viz::SurfaceId surface_id;
  if (!ReadParam(m, iter, &surface_id))
    return false;

  float device_scale_factor;
  if (!ReadParam(m, iter, &device_scale_factor))
    return false;

  gfx::Size size_in_pixels;
  if (!ReadParam(m, iter, &size_in_pixels))
    return false;

  *p = viz::SurfaceInfo(surface_id, device_scale_factor, size_in_pixels);
  return p->is_valid();
}

void ParamTraits<viz::SurfaceInfo>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceInfo(");
  LogParam(p.id(), l);
  l->append(", ");
  LogParam(p.device_scale_factor(), l);
  l->append(", ");
  LogParam(p.size_in_pixels(), l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#include "content/common/content_param_traits_macros.h"
}  // namespace IPC
