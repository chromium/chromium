// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/utils/extension_utils.h"

#include "base/strings/string_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

const ExtensionId& MaybeGetExtensionId(const Extension* extension) {
  return extension ? extension->id() : base::EmptyString();
}

}  // namespace extensions
