// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_POLICY_PARAMS_H_
#define SANDBOX_WIN_SRC_POLICY_PARAMS_H_

#include "sandbox/win/src/policy_engine_params.h"

namespace sandbox {

class ParameterSet;

// Warning: The following macros store the address to the actual variables, in
// other words, the values are not copied.
#define POLPARAMS_BEGIN(type) class type { public: enum Args {
#define POLPARAM(arg) arg,
#define POLPARAMS_END(type) PolParamLast }; }; \
  typedef sandbox::ParameterSet type##Array [type::PolParamLast];

// Policy parameters for file access.
POLPARAMS_BEGIN(OpenFile)
  POLPARAM(NAME)
  POLPARAM(ACCESS)
  POLPARAM(OPENONLY)
POLPARAMS_END(OpenFile)

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(NameBased)
  POLPARAM(NAME)
POLPARAMS_END(NameBased)

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_PARAMS_H_
