// Copyright 2012 The Chromium Authors
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

#include "content/common/content_export.h"
#include "content/common/content_param_traits_macros.h"
#include "ipc/ipc_mojo_param_traits.h"

namespace blink {
class MessagePortChannel;
class MessagePortDescriptor;
class PolicyValue;
}  // namespace blink

namespace ui {
class AXMode;
}

namespace viz {
class FrameSinkId;
class LocalSurfaceId;
class SurfaceId;
class SurfaceInfo;
}  // namespace viz

namespace IPC {

template <>
struct CONTENT_EXPORT ParamTraits<blink::MessagePortChannel> {
  typedef blink::MessagePortChannel param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<blink::MessagePortDescriptor> {
  typedef blink::MessagePortDescriptor param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<blink::PolicyValue> {
  typedef blink::PolicyValue param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
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

}  // namespace IPC

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_H_
