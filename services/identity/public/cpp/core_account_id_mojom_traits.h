// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_ID_MOJOM_TRAITS_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_ID_MOJOM_TRAITS_H_

#include <string>

#include "google_apis/gaia/core_account_id.h"
#include "services/identity/public/mojom/core_account_id.mojom.h"

namespace mojo {

template <>
struct StructTraits<identity::mojom::CoreAccountId::DataView, ::CoreAccountId> {
  static const std::string& id(const ::CoreAccountId& r) { return r.id; }

  static bool Read(identity::mojom::CoreAccountId::DataView data,
                   ::CoreAccountId* out);

  static bool IsNull(const ::CoreAccountId& input);

  static void SetToNull(::CoreAccountId* output) { *output = CoreAccountId(); }
};

}  // namespace mojo

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_CORE_ACCOUNT_ID_MOJOM_TRAITS_H_
