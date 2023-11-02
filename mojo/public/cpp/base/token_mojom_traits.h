// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_TOKEN_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_TOKEN_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/token.h"
#include "mojo/public/mojom/base/token.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::TokenDataView, base::Token> {
  static uint64_t high(const base::Token& token) { return token.high(); }
  static uint64_t low(const base::Token& token) { return token.low(); }
  static bool Read(mojo_base::mojom::TokenDataView data, base::Token* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_TOKEN_MOJOM_TRAITS_H_
