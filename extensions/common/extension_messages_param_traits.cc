// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_messages_param_traits.h"

#include "base/pickle.h"
#include "extensions/common/extension_messages.h"

namespace IPC {

using extensions::mojom::HostIDPtr;

void ParamTraits<HostIDPtr>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p->type);
  m->WriteString(p->id);
}

bool ParamTraits<HostIDPtr>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  param_type* p) {
  bool success = true;

  extensions::mojom::HostID::HostType type;
  success &= ReadParam(m, iter, &type);
  std::string id;
  success &= iter->ReadString(&id);

  if (success)
    *p = extensions::mojom::HostID::New(type, id);
  return success;
}

void ParamTraits<HostIDPtr>::Log(const param_type& p, std::string* l) {}

}  // namespace IPC
