// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_WAIT_H_
#define MOJO_PUBLIC_CPP_SYSTEM_WAIT_H_

#include <stddef.h>

#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Blocks the calling sequence, waiting for one or more signals in |signals| to
// be become satisfied, not-satisfied, or permanently unsatisfiable on the
// handle, depending on the |condition| selected.
//
// If |signals_state| is non-null, |handle| is valid, the wait is not cancelled
// (see return values below), the last known signaling state of |handle| is
// written to |*signals_state| before returning.
//
// Return values:
//   |MOJO_RESULT_OK| if one or more signals in |signals| has been raised on
//       |handle| with |condition| set to  |MOJO_WATCH_CONDITION_SATISFIED|, or
//       one or more signals in |signals| has been lowered on |handle| with
//       |condition| set to |MOJO_WATCH_CONDITION_NOT_SATISFIED|.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the state of |handle| changes such
//       that no signals in |signals| can ever be raised again and |condition|
//       is |MOJO_WATCH_CONDITION_SATISFIED|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//   |MOJO_RESULT_CANCELLED| if the wait was cancelled because |handle| was
//       closed by some other sequence while waiting.
MOJO_CPP_SYSTEM_EXPORT MojoResult
Wait(Handle handle,
     MojoHandleSignals signals,
     MojoTriggerCondition condition,
     MojoHandleSignalsState* signals_state = nullptr);

// A pseudonym for the above Wait() which always waits on
// |MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED|.
inline MojoResult Wait(Handle handle,
                       MojoHandleSignals signals,
                       MojoHandleSignalsState* signals_state = nullptr) {
  return Wait(handle, signals, MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
              signals_state);
}

// Waits on |handles[0]|, ..., |handles[num_handles-1]| until:
//  - At least one handle satisfies a signal indicated in its respective
//    |signals[0]|, ..., |signals[num_handles-1]|.
//  - It becomes known that no signal in some |signals[i]| will ever be
//    satisfied.
//
// This means that |WaitMany()| behaves as if |Wait()| were called on each
// handle/signals pair simultaneously, completing when the first |Wait()| would
// complete.
//
// If |signals_states| is non-null, all other arguments are valid, and the wait
// is not cancelled (see return values below), the last known signaling state of
// each Handle |handles[i]| is written to its corresponding entry in
// |signals_states[i]| before returning.
//
// Returns values:
//   |MOJO_RESULT_OK| if one of the Handles in |handles| had one or more of its
//       correpsonding signals satisfied. |*result_index| contains the index
//       of the Handle in question if |result_index| is non-null.
//   |MOJO_RESULT_FAILED_PRECONDITION| if one of the Handles in |handles|
//       changes state such that its corresponding signals become permanently
//       unsatisfiable. |*result_index| contains the index of the handle in
//       question if |result_index| is non-null.
//   |MOJO_RESULT_INVALID_ARGUMENT| if any Handle in |handles| is invalid,
//       or if either |handles| or |signals| is null.
//   |MOJO_RESULT_CANCELLED| if the wait was cancelled because a handle in
//       |handles| was closed by some other sequence while waiting.
//       |*result_index| contains the index of the closed Handle if
//       |result_index| is non-null.
MOJO_CPP_SYSTEM_EXPORT MojoResult
WaitMany(const Handle* handles,
         const MojoHandleSignals* signals,
         size_t num_handles,
         size_t* result_index = nullptr,
         MojoHandleSignalsState* signals_states = nullptr);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_WAIT_H_
