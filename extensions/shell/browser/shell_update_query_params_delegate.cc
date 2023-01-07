// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_update_query_params_delegate.h"

#include <string>

namespace extensions {

ShellUpdateQueryParamsDelegate::ShellUpdateQueryParamsDelegate() {
}

ShellUpdateQueryParamsDelegate::~ShellUpdateQueryParamsDelegate() {
}

std::string ShellUpdateQueryParamsDelegate::GetExtraParams() {
  // This version number is high enough to be supported by Omaha
  // (below 31 is unsupported), but it's fake enough to be obviously
  // not a Chrome release.
  return "&prodversion=38.1234.5678.9";
}

}  // namespace extensions
