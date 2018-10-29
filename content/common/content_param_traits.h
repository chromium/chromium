// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to define IPC::ParamTraits<> specializations for a number
// of types so that they can be serialized over IPC.  IPC::ParamTraits<>
// specializations for basic types (like int and std::string) and types in the
// 'base' project can be found in ipc/ipc_message_utils.h.  This file contains
// specializations for types that are used by the content code, and which need
// manual serialization code.  This is usually because they're not structs with
// public members, or because the same type is being used in multiple
// *_messages.h headers.

#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_H_

#include "content/common/content_param_traits_macros.h"
#include "content/common/cursors/webcursor.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
class MessagePortChannel;
struct TransferableMessage;
}

namespace content {
struct FrameMsg_ViewChanged_Params;
}

namespace viz {
class FrameSinkId;
class LocalSurfaceId;
class SurfaceId;
class SurfaceInfo;
}  // namespace viz

namespace IPC {

template <>
struct ParamTraits<content::WebCursor> {
  typedef content::WebCursor param_type;
  static void Write(base::Pickle* m, const param_type& p) { p.Serialize(m); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return r->Deserialize(iter);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<WebCursor>");
  }
};

typedef const blink::WebInputEvent* WebInputEventPointer;
template <>
struct ParamTraits<WebInputEventPointer> {
  typedef WebInputEventPointer param_type;
  static void Write(base::Pickle* m, const param_type& p);
  // Note: upon read, the event has the lifetime of the message.
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<blink::MessagePortChannel> {
  typedef blink::MessagePortChannel param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m, base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<ui::AXMode> {
  typedef ui::AXMode param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<scoped_refptr<storage::BlobHandle>> {
  typedef scoped_refptr<storage::BlobHandle> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<
    scoped_refptr<base::RefCountedData<blink::TransferableMessage>>> {
  typedef scoped_refptr<base::RefCountedData<blink::TransferableMessage>>
      param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<content::FrameMsg_ViewChanged_Params> {
  using param_type = content::FrameMsg_ViewChanged_Params;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<viz::FrameSinkId> {
  typedef viz::FrameSinkId param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<viz::LocalSurfaceId> {
  typedef viz::LocalSurfaceId param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<viz::SurfaceId> {
  typedef viz::SurfaceId param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<viz::SurfaceInfo> {
  typedef viz::SurfaceInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// TODO(tkent): Remove this when we switch to mojo. crbug.com/869257
template <>
struct CONTENT_EXPORT ParamTraits<blink::mojom::FileChooserFileInfoPtr> {
  using param_type = blink::mojom::FileChooserFileInfoPtr;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_H_
