// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/scoped_glib.h"

namespace remoting {

namespace internal {

void GVariantDeleter::operator()(GVariant* variant) {
  g_variant_unref(variant);
}

}  // namespace internal

ScopedGVariant TakeGVariant(GVariant* variant) {
  if (variant && g_variant_is_floating(variant)) {
    g_variant_ref_sink(variant);
  }
  return ScopedGVariant(variant);
}

}  // namespace remoting
