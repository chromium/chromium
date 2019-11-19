// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/scoped_worker_based_extensions_channel.h"

#include "extensions/common/constants.h"

namespace extensions {

ScopedWorkerBasedExtensionsChannel::ScopedWorkerBasedExtensionsChannel()
    : worker_based_extensions_channel_(
          extension_misc::kMinChannelForServiceWorkerBasedExtension) {}
ScopedWorkerBasedExtensionsChannel::~ScopedWorkerBasedExtensionsChannel() =
    default;

}  // namespace extensions
