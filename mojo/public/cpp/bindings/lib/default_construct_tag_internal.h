// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_DEFAULT_CONSTRUCT_TAG_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_DEFAULT_CONSTRUCT_TAG_INTERNAL_H_

#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace mojo::internal {

// This bit of indirection is so the `mojo::DefaultConstructTag` constructor can
// be private while the bindings layer can still easily use it. Code outside the
// mojo namespace should not be reaching into mojo::internal after all...
struct DefaultConstructTag {
  constexpr operator mojo::DefaultConstruct::Tag() const {
    return mojo::DefaultConstruct::Tag();
  }
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_DEFAULT_CONSTRUCT_TAG_INTERNAL_H_
