// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains basic functions common to different Mojo system APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
#define MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes Mojo in the calling application.
//
// With the exception of Mojo Core embedders, applications using Mojo APIs must
// call this function before any others.
//
// |options| may be null.
//
// If the |MOJO_INITIALIZE_FLAG_LOAD_ONLY| flag is given in |options|, this only
// partially initializes the library. Other Mojo APIs will remain unavailable
// until the library is fully initialized by a subsequent call to
// |MojoInitialize()| WITHOUT the flag. See documentation on
// |MOJO_INITIALIZE_FLAG_LOAD_ONLY| in types.h for details.
//
// Returns:
//   |MOJO_RESULT_OK| if Mojo initialization was successful.
//   |MOJO_RESULT_NOT_FOUND| if the Mojo Core library could not be loaded or
//       appears to be malformed.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the Mojo Core library AND full IPC
//       support has already been initialized by some prior call(s) to
//       |MojoInitialize()|.
//   |MOJO_RESULT_ALREADY_EXISTS| if |MOJO_INITIALIZE_FLAG_LOAD_ONLY| was
//       specified for this call but the library has already been successfully
//       loaded and partially initialized by a previous call with the same flag.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |options| was non-null and invalid.
MOJO_SYSTEM_EXPORT MojoResult
MojoInitialize(const struct MojoInitializeOptions* options);

// Shuts down Mojo in the calling application.
//
// This should only be called if |MojoInitialize()| was also called at some
// point in the calling process. It therefore only applies to consumers of Mojo
// as a shared library, not Mojo Core embedders.
//
// |options| may be null.
//
// NOTE: It is NOT safe to attempt to call |MojoInitialize()| again (or any
// other Mojo APIs, for that matter) after calling |MojoShutdown()|.
//
// Returns:
//   |MOJO_RESULT_OK| if shutdown was successful.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |options| was non-null and invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |MojoInitialize()| was never called.
//   |MOJO_RESULT_UNIMPLEMENTED| if the caller is a Mojo Core embedder and is
//       therefore not loading Mojo Core as a shared library.
MOJO_SYSTEM_EXPORT MojoResult
MojoShutdown(const struct MojoShutdownOptions* options);

// Returns the time, in microseconds, since some undefined point in the past.
// The values are only meaningful relative to other values that were obtained
// from the same device without an intervening system restart. Such values are
// guaranteed to be monotonically non-decreasing with the passage of real time.
// Although the units are microseconds, the resolution of the clock may vary and
// is typically in the range of ~1-15 ms.
MOJO_SYSTEM_EXPORT MojoTimeTicks MojoGetTimeTicksNow(void);

// Closes the given |handle|.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//
// Concurrent operations on |handle| may succeed (or fail as usual) if they
// happen before the close, be cancelled with result |MOJO_RESULT_CANCELLED| if
// they properly overlap (this is likely the case with traps), or fail with
// |MOJO_RESULT_INVALID_ARGUMENT| if they happen after.
MOJO_SYSTEM_EXPORT MojoResult MojoClose(MojoHandle handle);

// Queries the last known signals state of a handle.
//
// Note that no guarantees can be made about the accuracy of the returned
// signals state by the time this returns, as other threads in the system may
// change the handle's state at any time. Use with appropriate discretion.
//
// Returns:
//   |MOJO_RESULT_OK| on success. |*signals_state| is populated with the
//       last known signals state of |handle|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle or
//       |signals_state| is null.
MOJO_SYSTEM_EXPORT MojoResult
MojoQueryHandleSignalsState(MojoHandle handle,
                            struct MojoHandleSignalsState* signals_state);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
