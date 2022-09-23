// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_PARAM_TRAITS_H_
#define EXTENSIONS_COMMON_EXTENSION_PARAM_TRAITS_H_

#include "extensions/common/mojom/extra_response_data.mojom.h"
#include "ipc/ipc_mojo_param_traits.h"

namespace IPC {

template <>
struct ParamTraits<extensions::mojom::ExtraResponseDataPtr> {
  typedef extensions::mojom::ExtraResponseDataPtr param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // EXTENSIONS_COMMON_EXTENSION_PARAM_TRAITS_H_
