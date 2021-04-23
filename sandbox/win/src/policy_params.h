// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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

// Policy parameters for file open / create.
POLPARAMS_BEGIN(OpenFile)
  POLPARAM(NAME)
  POLPARAM(BROKER)  // true if called from the broker.
  POLPARAM(ACCESS)
  POLPARAM(DISPOSITION)
  POLPARAM(OPTIONS)
POLPARAMS_END(OpenFile)

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(FileName)
  POLPARAM(NAME)
  POLPARAM(BROKER)  // true if called from the broker.
POLPARAMS_END(FileName)

static_assert(OpenFile::NAME == static_cast<int>(FileName::NAME),
              "to simplify fs policies");
static_assert(OpenFile::BROKER == static_cast<int>(FileName::BROKER),
              "to simplify fs policies");

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(NameBased)
  POLPARAM(NAME)
POLPARAMS_END(NameBased)

// Policy parameters for open event.
POLPARAMS_BEGIN(OpenEventParams)
  POLPARAM(NAME)
  POLPARAM(ACCESS)
POLPARAMS_END(OpenEventParams)

// Policy Parameters for reg open / create.
POLPARAMS_BEGIN(OpenKey)
  POLPARAM(NAME)
  POLPARAM(ACCESS)
POLPARAMS_END(OpenKey)

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(HandleTarget)
  POLPARAM(NAME)
  POLPARAM(TARGET)
POLPARAMS_END(HandleTarget)

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_PARAMS_H_
