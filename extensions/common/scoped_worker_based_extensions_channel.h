// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SCOPED_WORKER_BASED_EXTENSIONS_CHANNEL_H_
#define EXTENSIONS_COMMON_SCOPED_WORKER_BASED_EXTENSIONS_CHANNEL_H_

#include "extensions/common/features/feature_channel.h"

namespace extensions {

// Sets the current channel so that Service Worker based extensions are enabled
// and also and extension APIs from those Service Workers are enabled.
//
// Note: It is important to set this override early in tests so that the
// channel change is visible to renderers running with service workers.
class ScopedWorkerBasedExtensionsChannel {
 public:
  ScopedWorkerBasedExtensionsChannel();
  ~ScopedWorkerBasedExtensionsChannel();

 private:
  ScopedCurrentChannel worker_based_extensions_channel_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWorkerBasedExtensionsChannel);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SCOPED_WORKER_BASED_EXTENSIONS_CHANNEL_H_
