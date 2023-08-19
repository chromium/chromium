// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <string>

#include "content/public/common/content_constants.h"
#include "content/public/common/referrer.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/page_state/page_state.h"

namespace IPC {

void ParamTraits<blink::PageState>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.ToEncodedData());
}

bool ParamTraits<blink::PageState>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* r) {
  std::string data;
  if (!ReadParam(m, iter, &data))
    return false;
  *r = blink::PageState::CreateFromEncodedData(data);
  return true;
}

void ParamTraits<blink::PageState>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.ToEncodedData(), l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#include "content/public/common/common_param_traits_macros.h"
}  // namespace IPC
