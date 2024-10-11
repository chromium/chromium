// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_type.h"

#include <glib.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace remoting::gvariant {

const GVariantType* TypeBase::gvariant_type() const {
  return G_VARIANT_TYPE(contents().first);
}

TypeBase::operator const GVariantType*() const {
  return gvariant_type();
}

Type<>::Type(const GVariantType* type)
    : type_string_(g_variant_type_peek_string(type),
                   g_variant_type_get_string_length(type)) {}

}  // namespace remoting::gvariant
