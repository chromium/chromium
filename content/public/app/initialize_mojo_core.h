// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_INITIALIZE_MOJO_CORE_H_
#define CONTENT_PUBLIC_APP_INITIALIZE_MOJO_CORE_H_

#include "content/common/content_export.h"

namespace content {

// Initializes the Mojo Core library within the calling process. This makes Mojo
// APIs usable in the calling process, and it installs shared memory allocation
// hooks if necessary, allowing sandboxed processes to allocate shared memory
// synchronously through Mojo.
//
// This must be called by all Content processes early in startup -- specifically
// before any Mojo APIs are used, but after FeatureList has been initialized.
//
// By default, Content processes call this immediately after they initialize the
// FeatureList, but embedders may change either behavior by overriding
// ShouldCreateFeatureList() and/or ShouldInitializeMojo() in
// ContentMainDelegate. Embedders who override the latter can call this function
// to initialize Mojo when desired.
void CONTENT_EXPORT InitializeMojoCore();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_INITIALIZE_MOJO_CORE_H_
