// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_VAR_DEPRECATED_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_VAR_DEPRECATED_IMPL_H_

struct PPB_Var_Deprecated;

namespace content {

class PPB_Var_Deprecated_Impl {
 public:
  static const PPB_Var_Deprecated* GetVarDeprecatedInterface();
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_VAR_DEPRECATED_IMPL_H_
