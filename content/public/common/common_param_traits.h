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

#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_

#include <stdint.h>

#include <string>

#include "base/notreached.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"
#include "url/ipc/url_param_traits.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace blink {
class PageState;
}

namespace IPC {

template <>
struct CONTENT_EXPORT ParamTraits<blink::PageState> {
  typedef blink::PageState param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::NativeWindow> {
  typedef gfx::NativeWindow param_type;
  static void Write(base::Pickle* m, const param_type& p) {
#if BUILDFLAG(IS_WIN)
    m->WriteUInt32(base::win::HandleToUint32(p));
#else
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(p));
#endif
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
#if BUILDFLAG(IS_WIN)
    return iter->ReadUInt32(reinterpret_cast<uint32_t*>(r));
#else
    const char *data;
    size_t data_size = 0;
    bool result = iter->ReadData(&data, &data_size);
    if (result && data_size == sizeof(gfx::NativeWindow)) {
      memcpy(r, data, sizeof(gfx::NativeWindow));
    } else {
      result = false;
      NOTREACHED_IN_MIGRATION();
    }
    return result;
#endif
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<gfx::NativeWindow>");
  }
};

}  // namespace IPC

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_H_
