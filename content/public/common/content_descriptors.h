// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_

#include "build/build_config.h"

#include "services/service_manager/embedder/descriptors.h"

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
#if defined(OS_ANDROID)
  kAndroidPropertyDescriptor = service_manager::kFirstEmbedderDescriptor,
  kAndroidICUDataDescriptor,
  kAndroidICUExtraDataDescriptor,
#endif

  // Reserves 100 to 199 for dynamically generated IDs.
  kContentDynamicDescriptorStart = 100,
  kContentDynamicDescriptorMax = 199,

  // The first key that embedders can use to register descriptors (see
  // base/posix/global_descriptors.h).
  kContentIPCDescriptorMax
};

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
