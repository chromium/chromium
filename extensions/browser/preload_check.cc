// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/preload_check.h"

#include "extensions/common/extension.h"

namespace extensions {

PreloadCheck::PreloadCheck(scoped_refptr<const Extension> extension)
    : extension_(extension) {}

PreloadCheck::~PreloadCheck() = default;

std::u16string PreloadCheck::GetErrorMessage() const {
  return std::u16string();
}

}  // namespace extensions
