// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_
#define EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_

#include "base/memory/raw_ptr.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handler_registry.h"

namespace extensions {

class ScopedTestingManifestHandlerRegistry {
 public:
  ScopedTestingManifestHandlerRegistry();
  ~ScopedTestingManifestHandlerRegistry();

  // TODO(devlin): Provide an accessor for |registry_|.

 private:
  ManifestHandlerRegistry registry_;
  raw_ptr<ManifestHandlerRegistry> old_registry_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SCOPED_TESTING_MANIFEST_HANDLER_REGISTRY_H_
