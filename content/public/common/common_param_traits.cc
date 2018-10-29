// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <string>

#include "base/containers/stack_container.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "net/http/http_util.h"

namespace IPC {

void ParamTraits<content::PageState>::Write(base::Pickle* m,
                                            const param_type& p) {
  WriteParam(m, p.ToEncodedData());
}

bool ParamTraits<content::PageState>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
  std::string data;
  if (!ReadParam(m, iter, &data))
    return false;
  *r = content::PageState::CreateFromEncodedData(data);
  return true;
}

void ParamTraits<content::PageState>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.ToEncodedData(), l);
  l->append(")");
}

void ParamTraits<ui::AXTreeID>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.ToString());
}

bool ParamTraits<ui::AXTreeID>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  std::string value;
  if (!ReadParam(m, iter, &value))
    return false;
  *r = ui::AXTreeID::FromString(value);
  return true;
}

void ParamTraits<ui::AXTreeID>::Log(const param_type& p, std::string* l) {
  l->append("<ui::AXTreeID>");
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
