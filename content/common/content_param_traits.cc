// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/common/frame_message_structs.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "net/base/ip_endpoint.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "url/ipc/url_param_traits.h"
// #include "ui/gfx/ipc/geometry/gfx_param_traits.h"

namespace IPC {

void ParamTraits<WebInputEventPointer>::Write(base::Pickle* m,
                                              const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p), p->size());
}

bool ParamTraits<WebInputEventPointer>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  const char* data;
  int data_length;
  if (!iter->ReadData(&data, &data_length)) {
    NOTREACHED();
    return false;
  }
  if (data_length < static_cast<int>(sizeof(blink::WebInputEvent))) {
    NOTREACHED();
    return false;
  }
  param_type event = reinterpret_cast<param_type>(data);
  // Check that the data size matches that of the event.
  if (data_length != static_cast<int>(event->size())) {
    NOTREACHED();
    return false;
  }
  const size_t expected_size_for_type =
      ui::WebInputEventTraits::GetSize(event->GetType());
  if (data_length != static_cast<int>(expected_size_for_type)) {
    NOTREACHED();
    return false;
  }
  *r = event;
  return true;
}

void ParamTraits<WebInputEventPointer>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  LogParam(p->size(), l);
  l->append(", ");
  LogParam(p->GetType(), l);
  l->append(", ");
  LogParam(p->TimeStamp(), l);
  l->append(")");
}

void ParamTraits<blink::MessagePortChannel>::Write(base::Pickle* m,
                                                   const param_type& p) {
  ParamTraits<mojo::MessagePipeHandle>::Write(m, p.ReleaseHandle().release());
}

bool ParamTraits<blink::MessagePortChannel>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* r) {
  mojo::MessagePipeHandle handle;
  if (!ParamTraits<mojo::MessagePipeHandle>::Read(m, iter, &handle))
    return false;
  *r = blink::MessagePortChannel(mojo::ScopedMessagePipeHandle(handle));
  return true;
}

void ParamTraits<blink::MessagePortChannel>::Log(const param_type& p,
                                                 std::string* l) {}

void ParamTraits<ui::AXMode>::Write(base::Pickle* m, const param_type& p) {
  IPC::WriteParam(m, p.mode());
}

bool ParamTraits<ui::AXMode>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* r) {
  uint32_t value;
  if (!IPC::ReadParam(m, iter, &value))
    return false;
  *r = ui::AXMode(value);
  return true;
}

void ParamTraits<ui::AXMode>::Log(const param_type& p, std::string* l) {}

void ParamTraits<scoped_refptr<storage::BlobHandle>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p != nullptr);
  if (p) {
    auto info = p->Clone().PassInterface();
    m->WriteUInt32(info.version());
    MojoMessageHelper::WriteMessagePipeTo(m, info.PassHandle());
  }
}

bool ParamTraits<scoped_refptr<storage::BlobHandle>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool is_not_null;
  if (!ReadParam(m, iter, &is_not_null))
    return false;
  if (!is_not_null)
    return true;

  uint32_t version;
  if (!ReadParam(m, iter, &version))
    return false;
  mojo::ScopedMessagePipeHandle handle;
  if (!MojoMessageHelper::ReadMessagePipeFrom(m, iter, &handle))
    return false;
  DCHECK(handle.is_valid());
  blink::mojom::BlobPtr blob;
  blob.Bind(blink::mojom::BlobPtrInfo(std::move(handle), version));
  *r = base::MakeRefCounted<storage::BlobHandle>(std::move(blob));
  return true;
}

void ParamTraits<scoped_refptr<storage::BlobHandle>>::Log(const param_type& p,
                                                          std::string* l) {
  l->append("<storage::BlobHandle>");
}

// static
void ParamTraits<content::FrameMsg_ViewChanged_Params>::Write(
    base::Pickle* m,
    const param_type& p) {
  DCHECK(features::IsMultiProcessMash() ||
         (p.frame_sink_id.has_value() && p.frame_sink_id->is_valid()));
  WriteParam(m, p.frame_sink_id);
}

bool ParamTraits<content::FrameMsg_ViewChanged_Params>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  if (!ReadParam(m, iter, &(r->frame_sink_id)))
    return false;
  if (!features::IsMultiProcessMash() &&
      (!r->frame_sink_id || !r->frame_sink_id->is_valid())) {
    NOTREACHED();
    return false;
  }
  return true;
}

// static
void ParamTraits<content::FrameMsg_ViewChanged_Params>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("(");
  LogParam(p.frame_sink_id, l);
  l->append(")");
}

template <>
struct ParamTraits<blink::mojom::SerializedBlobPtr> {
  using param_type = blink::mojom::SerializedBlobPtr;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p->uuid);
    WriteParam(m, p->content_type);
    WriteParam(m, p->size);
    WriteParam(m, p->blob.PassHandle().release());
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
    (*r)->blob = blink::mojom::BlobPtrInfo(
        mojo::ScopedMessagePipeHandle(handle), blink::mojom::Blob::Version_);
    return true;
  }
};

void ParamTraits<scoped_refptr<base::RefCountedData<
    blink::TransferableMessage>>>::Write(base::Pickle* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(p->data.encoded_message.data()),
               p->data.encoded_message.size());
  WriteParam(m, p->data.blobs);
  WriteParam(m, p->data.stack_trace_id);
  WriteParam(m, p->data.stack_trace_debugger_id_first);
  WriteParam(m, p->data.stack_trace_debugger_id_second);
  WriteParam(m, p->data.ports);
  WriteParam(m, p->data.has_user_gesture);
  WriteParam(m, !!p->data.user_activation);
  if (p->data.user_activation) {
    WriteParam(m, p->data.user_activation->has_been_active);
    WriteParam(m, p->data.user_activation->was_active);
  }
}

bool ParamTraits<
    scoped_refptr<base::RefCountedData<blink::TransferableMessage>>>::
    Read(const base::Pickle* m, base::PickleIterator* iter, param_type* r) {
  *r = new base::RefCountedData<blink::TransferableMessage>();

  const char* data;
  int length;
  if (!iter->ReadData(&data, &length))
    return false;
  // This just makes encoded_message point into the IPC message buffer. Usually
  // code receiving a TransferableMessage will synchronously process the message
  // so this avoids an unnecessary copy. If a receiver needs to hold on to the
  // message longer, it should make sure to call EnsureDataIsOwned on the
  // returned message.
  (*r)->data.encoded_message =
      base::make_span(reinterpret_cast<const uint8_t*>(data), length);
  bool has_activation = false;
  if (!ReadParam(m, iter, &(*r)->data.blobs) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_id) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_first) ||
      !ReadParam(m, iter, &(*r)->data.stack_trace_debugger_id_second) ||
      !ReadParam(m, iter, &(*r)->data.ports) ||
      !ReadParam(m, iter, &(*r)->data.has_user_gesture) ||
      !ReadParam(m, iter, &has_activation)) {
    return false;
  }

  if (has_activation) {
    bool has_been_active;
    bool was_active;
    if (!ReadParam(m, iter, &has_been_active) ||
        !ReadParam(m, iter, &was_active)) {
      return false;
    }
    (*r)->data.user_activation =
        blink::mojom::UserActivationSnapshot::New(has_been_active, was_active);
  }
  return true;
}

void ParamTraits<scoped_refptr<
    base::RefCountedData<blink::TransferableMessage>>>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("<blink::TransferableMessage>");
}

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

void ParamTraits<blink::mojom::FileChooserFileInfoPtr>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p->is_native_file());
  if (p->is_native_file()) {
    WriteParam(m, p->get_native_file()->file_path);
    WriteParam(m, p->get_native_file()->display_name);
  } else {
    WriteParam(m, p->get_file_system()->url);
    WriteParam(m, p->get_file_system()->modification_time);
    WriteParam(m, p->get_file_system()->length);
  }
}

bool ParamTraits<blink::mojom::FileChooserFileInfoPtr>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  bool is_native;
  if (!ReadParam(m, iter, &is_native))
    return false;

  if (is_native) {
    base::FilePath file_path;
    if (!ReadParam(m, iter, &file_path))
      return false;

    base::string16 display_name;
    if (!ReadParam(m, iter, &display_name))
      return false;

    *p = blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_path, display_name));
  } else {
    GURL file_system_url;
    if (!ReadParam(m, iter, &file_system_url))
      return false;

    base::Time modification_time;
    if (!ReadParam(m, iter, &modification_time))
      return false;

    int64_t length;
    if (!ReadParam(m, iter, &length))
      return false;

    *p = blink::mojom::FileChooserFileInfo::NewFileSystem(
        blink::mojom::FileSystemFileInfo::New(file_system_url,
                                              modification_time, length));
  }
  return true;
}

void ParamTraits<blink::mojom::FileChooserFileInfoPtr>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("blink::mojom::FileChooserFileInfo(");
  if (p->is_native_file()) {
    l->append("NativeFileInfo(");
    LogParam(p->get_native_file()->file_path, l);
    l->append(", ");
    LogParam(p->get_native_file()->display_name, l);
  } else {
    l->append("FileSystemFileInfo(");
    LogParam(p->get_file_system()->url, l);
    l->append(", ");
    LogParam(p->get_file_system()->modification_time, l);
    l->append(", ");
    LogParam(p->get_file_system()->length, l);
  }
  l->append("))");
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
