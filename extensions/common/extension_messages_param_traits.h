// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1180858): These traits files,
// extension_messages_param_traits.{cc,h}, are required for sending extension
// mojom types to legacy IPCs. Once the mojofication of extension is done, these
// traits should be removed.

#ifndef EXTENSIONS_COMMON_EXTENSION_MESSAGES_PARAM_TRAITS_H_
#define EXTENSIONS_COMMON_EXTENSION_MESSAGES_PARAM_TRAITS_H_

#include "extensions/common/mojom/host_id.mojom.h"

#include "ipc/ipc_message_utils.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace IPC {

template <>
struct ParamTraits<extensions::mojom::HostIDPtr> {
  typedef extensions::mojom::HostIDPtr param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // EXTENSIONS_COMMON_EXTENSION_MESSAGES_PARAM_TRAITS_H_
