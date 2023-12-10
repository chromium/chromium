// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SCOPED_GLIB_H_
#define REMOTING_HOST_LINUX_SCOPED_GLIB_H_

#include <gio/gio.h>

#include <memory>

// This file defines some scoper classes and utilities for some GLib types that
// are not derived from GObject.

namespace remoting {

namespace internal {

struct GVariantDeleter {
  void operator()(GVariant* variant);
};

}  // namespace internal

using ScopedGVariant = std::unique_ptr<GVariant, internal::GVariantDeleter>;

// Takes ownership of a GVariant reference. If the reference is floating, it
// will be sunk.
ScopedGVariant TakeGVariant(GVariant* variant);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SCOPED_GLIB_H_
