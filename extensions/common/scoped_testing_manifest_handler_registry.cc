// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/scoped_testing_manifest_handler_registry.h"

namespace extensions {

ScopedTestingManifestHandlerRegistry::ScopedTestingManifestHandlerRegistry() {
  old_registry_ = ManifestHandlerRegistry::SetForTesting(&registry_);
}

ScopedTestingManifestHandlerRegistry::~ScopedTestingManifestHandlerRegistry() {
  ManifestHandlerRegistry::SetForTesting(old_registry_);
}

}  // namespace extensions
