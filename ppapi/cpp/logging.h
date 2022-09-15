// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_LOGGING_H_
#define PPAPI_CPP_LOGGING_H_

/// @file
/// This file defines two macro asserts.

#include <cassert>

/// This macro asserts that 'a' evaluates to true. In debug mode, this macro
/// will crash the program if the assertion evaluates to false. It (typically)
/// has no effect in release mode.
#define PP_DCHECK(a) assert(a)

/// This macro asserts false in debug builds. It's used in code paths that you
/// don't expect to execute.
///
/// <strong>Example:</strong>
///
/// @code
/// if (!pointer) {
/// // Pointer wasn't valid! This shouldn't happen.
/// PP_NOTREACHED();
/// return;
/// }
/// // Do stuff to the pointer, since you know it's valid.
/// pointer->DoSomething();
/// @endcode
#define PP_NOTREACHED() assert(false)

#endif  // PPAPI_CPP_LOGGING_H_
