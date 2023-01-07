// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "components/update_client/update_query_params_delegate.h"

namespace extensions {

class ShellUpdateQueryParamsDelegate
    : public update_client::UpdateQueryParamsDelegate {
 public:
  ShellUpdateQueryParamsDelegate();

  ShellUpdateQueryParamsDelegate(const ShellUpdateQueryParamsDelegate&) =
      delete;
  ShellUpdateQueryParamsDelegate& operator=(
      const ShellUpdateQueryParamsDelegate&) = delete;

  ~ShellUpdateQueryParamsDelegate() override;

  std::string GetExtraParams() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_UPDATE_QUERY_PARAMS_DELEGATE_H_
