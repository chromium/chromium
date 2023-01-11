// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the standalone functions of the Mojo
// C API, replacing the prefix of "Mojo" with a "mojo" namespace.
//
// Please see "mojo/public/c/system/functions.h" for complete documentation of
// the API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FUNCTIONS_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FUNCTIONS_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Returns the current |MojoTimeTicks| value. See |MojoGetTimeTicksNow()| for
// complete documentation.
inline MojoTimeTicks GetTimeTicksNow() {
  return MojoGetTimeTicksNow();
}

// Sets a callback to handle communication errors regarding peer processes whose
// identity is not explicitly known by this process, i.e. processes that are
// part of the same Mojo process network but which were not invited by this
// process.
//
// This can be used to globally listen for reports of bad incoming IPCs.
using DefaultProcessErrorHandler =
    base::RepeatingCallback<void(const std::string& error)>;
void MOJO_CPP_SYSTEM_EXPORT
SetDefaultProcessErrorHandler(DefaultProcessErrorHandler handler);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FUNCTIONS_H_
