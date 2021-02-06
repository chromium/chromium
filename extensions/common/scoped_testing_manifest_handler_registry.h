// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_
#define EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_

#include "extensions/common/manifest_handler.h"

namespace extensions {

class ScopedTestingManifestHandlerRegistry {
 public:
  ScopedTestingManifestHandlerRegistry();
  ~ScopedTestingManifestHandlerRegistry();

  // TODO(devlin): Provide an accessor for |registry_|.

 private:
  ManifestHandlerRegistry registry_;
  ManifestHandlerRegistry* old_registry_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_
