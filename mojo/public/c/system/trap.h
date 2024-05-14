// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_TRAP_H_
#define MOJO_PUBLIC_C_SYSTEM_TRAP_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Flags passed to trap event handlers within |MojoTrapEvent|.
typedef uint32_t MojoTrapEventFlags;

#ifdef __cplusplus
inline constexpr MojoTrapEventFlags MOJO_TRAP_EVENT_FLAG_NONE = 0;
inline constexpr MojoTrapEventFlags MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL = 1
                                                                           << 0;
#else
#define MOJO_TRAP_EVENT_FLAG_NONE ((MojoTrapEventFlags)0)
#define MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL ((MojoTrapEventFlags)1 << 0)
#endif

// Structure passed to trap event handlers when invoked by a tripped trap.
struct MOJO_ALIGNAS(8) MojoTrapEvent {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // May take on some combination of the following values:
  //
  //   |MOJO_TRAP_EVENT_FLAG_NONE|: No flags.
  //
  //   |MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL|: The trap was tripped within the
  //       extent of a user call to some Mojo API. This means that the event
  //       handler itself is re-entering user code. May happen, for example, if
  //       user code writes to an intra-process pipe and the receiving end trips
  //       a trap as a result. In that case the event handler executes within
  //       the extent of the |MojoWriteMessage()| call.
  MojoTrapEventFlags flags;

  // The context for the trigger which tripped the trap.
  MOJO_POINTER_FIELD(uintptr_t, trigger_context);

  // A result code indicating the cause of the event. May take on any of the
  // following values:
  //
  //   |MOJO_RESULT_OK|: The trigger's conditions were met.
  //   |MOJO_RESULT_FAILED_PRECONDITION|: The trigger's observed handle has
  //       changed state in such a way that the trigger's conditions can never
  //       be met again.
  //   |MOJO_RESULT_CANCELLED|: The trigger has been removed and will never
  //       cause another event to fire. This is always the last event fired by
  //       a trigger and it will fire when: the trigger is explicitly removed
  //       with |MojoRemoteTrigger()|, the trigger's owning trap handle is
  //       closed, or the handle observed by the trigger is closed.
  //
  //       Unlike the other result types above |MOJO_RESULT_CANCELLED| can
  //       fire even when the trap is disarmed.
  MojoResult result;

  // The last known signalling state of the trigger's observed handle at the
  // time the trap was tripped.
  struct MojoHandleSignalsState signals_state;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoTrapEvent) == 32,
                   "MojoTrapEvent has wrong size.");

// Value given to |MojoAddTrigger| to configure what condition should cause it
// to trip its trap. May be one of the following values:
//
//   |MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED| - A trigger added with this
//       condition will trip its trap when any of its observed signals
//       transition from being satisfied to being unsatisfied.
//   |MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED| - A triger added with this
//       condition will trip its trap  when any of its observed signals
//       transition from being unsatisfied to being satisfied, or when none of
//       the observed signals can ever be satisfied again.
typedef uint32_t MojoTriggerCondition;

#ifdef __cplusplus
inline constexpr MojoTriggerCondition
    MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED = 0;
inline constexpr MojoTriggerCondition MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED =
    1;
#else
#define MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED ((MojoTriggerCondition)0)
#define MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED ((MojoTriggerCondition)1)
#endif

// Flags passed to |MojoCreateTrap()| via |MojoCreateTrapOptions|.
typedef uint32_t MojoCreateTrapFlags;

#ifdef __cplusplus
inline constexpr MojoCreateTrapFlags MOJO_CREATE_TRAP_FLAG_NONE = 0;
#else
#define MOJO_CREATE_TRAP_FLAG_NONE ((MojoCreateTrapFlags)0)
#endif

// Options passed to |MojoCreateTrap()|.
struct MOJO_ALIGNAS(8) MojoCreateTrapOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // Flags. Currently unused.
  MojoCreateTrapFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoCreateTrapOptions) == 8,
                   "MojoCreateTrapOptions has wrong size.");

// Flags passed to |MojoAddTrigger()| via |MojoAddTriggerOptions|.
typedef uint32_t MojoAddTriggerFlags;

#ifdef __cplusplus
inline constexpr MojoAddTriggerFlags MOJO_ADD_TRIGGER_FLAG_NONE = 0;
#else
#define MOJO_ADD_TRIGGER_FLAG_NONE ((MojoAddTriggerFlags)0)
#endif

// Options passed to |MojoAddTrigger()|.
struct MOJO_ALIGNAS(8) MojoAddTriggerOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // Flags. Currently unused.
  MojoAddTriggerFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoAddTriggerOptions) == 8,
                   "MojoAddTriggerOptions has wrong size.");

// Flags passed to |MojoRemoveTrigger()| via |MojoRemoveTriggerOptions|.
typedef uint32_t MojoRemoveTriggerFlags;

#ifdef __cplusplus
inline constexpr MojoRemoveTriggerFlags MOJO_REMOVE_TRIGGER_FLAG_NONE = 0;
#else
#define MOJO_REMOVE_TRIGGER_FLAG_NONE ((MojoRemoveTriggerFlags)0)
#endif

// Options passed to |MojoRemoveTrigger()|.
struct MOJO_ALIGNAS(8) MojoRemoveTriggerOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // Flags. Currently unused.
  MojoRemoveTriggerFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoRemoveTriggerOptions) == 8,
                   "MojoRemoveTriggerOptions has wrong size.");

// Flags passed to |MojoArmTrap()| via |MojoArmTrapOptions|.
typedef uint32_t MojoArmTrapFlags;

#ifdef __cplusplus
inline constexpr MojoArmTrapFlags MOJO_ARM_TRAP_FLAG_NONE = 0;
#else
#define MOJO_ARM_TRAP_FLAG_NONE ((MojoArmTrapFlags)0)
#endif

// Options passed to |MojoArmTrap()|.
struct MOJO_ALIGNAS(8) MojoArmTrapOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // Flags. Currently unused.
  MojoArmTrapFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoArmTrapOptions) == 8,
                   "MojoArmTrapOptions has wrong size.");

#ifdef __cplusplus
extern "C" {
#endif

// A user-provided callback to handle trap events. Passed to |MojoCreateTrap()|.
typedef void (*MojoTrapEventHandler)(const struct MojoTrapEvent* event);

// Creates a new trap which can be used to detect signal changes on a handle.
// Traps execute arbitrary user code when tripped.
//
// Traps can trip only while armed**, and new traps are created in a disarmed
// state. Traps may be armed using |MojoArmTrap()|.
//
// Arming a trap is only possible when the trap has one or more triggers
// attached to it. Triggers can be added or removed using |MojoAddTrigger()| and
// |MojoRemoveTrigger()|.
//
// If a trap is tripped by any of its triggers, it is disarmed immediately and
// the traps |MojoTrapEventHandler| is invoked once for every relevant trigger.
//
// |options| may be null.
//
// ** An unarmed trap will still fire an event when a trigger is removed. This
// event will always convey the result |MOJO_RESULT_CANCELLED|.
//
// Parameters:
//   |handler|: The |MojoTrapEventHandler| to invoke any time this trap is
//       tripped. Note that this may be called from any arbitrary thread.
//   |trap_handle|: The address at which to store the MojoHandle corresponding
//       to the new trap if successfully created.
//
// Returns:
//   |MOJO_RESULT_OK| if the trap has been successfully created.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a handle could not be allocated for
//       this trap.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateTrap(MojoTrapEventHandler handler,
               const struct MojoCreateTrapOptions* options,
               MojoHandle* trap_handle);

// Adds a trigger to a trap. This configures the trap to invoke its event
// handler if the specified conditions are met (or can no longer be met) while
// the trap is armed.
//
// Note that event handler invocations for a given trigger are mutually
// exclusive in execution: the handler will never be entered for a trigger while
// another thread is executing it for the same trigger. Similarly, event
// handlers are never re-entered. If an event handler changes the state of the
// system such that another event would fire, that event is deferred until the
// first handler returns.
//
// Parameters:
//   |trap_handle|: The trap to which this trigger is to be added.
//   |handle|: The handle whose signals this trigger will observe. Must be a
//       message pipe or data pipe handle.
//   |signals|: The specific signal(s) this trigger will observe on |handle|.
//   |condition|: The signaling condition this trigger will observe. i.e.
//       whether to trip the trap when |signals| become satisfied or when they
//       become unsatisfied.
//   |context|: An arbitrary context value to be passed to the trap's event
//       handler when this trigger was responsible for tripping the trap. See
//       the |trigger_context| field in |MojoTrapEvent|. This value must be
//       unique among all triggers on the trap.
//
//   |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| if the handle is now being observed by the trigger.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |trap_handle| is not a trap handle,
//       |handle| is not a valid message pipe or data pipe handle, or |signals|
//       or |condition| are an invalid value.
//   |MOJO_RESULT_ALREADY_EXISTS| if the trap already has a trigger associated
//       with |context| or |handle|.
MOJO_SYSTEM_EXPORT MojoResult
MojoAddTrigger(MojoHandle trap_handle,
               MojoHandle handle,
               MojoHandleSignals signals,
               MojoTriggerCondition condition,
               uintptr_t context,
               const struct MojoAddTriggerOptions* options);

// Removes a trigger from a trap.
//
// This ensures that the trigger is removed as soon as possible. Removal may
// block an arbitrarily long time if the trap is already executing its handler.
//
// When removal is complete, the trap's handler is invoked one final time for
// time for |context|, with the result |MOJO_RESULT_CANCELLED|.
//
// The same  behavior can be elicted by either closing the watched handle
// associated with this trigger, or by closing |trap_handle| itself.
//
// Parameters:
//   |trap_handle|: The handle of the trap from which to remove a trigger.
//   |context|: The context of the trigger to be removed.
//
// Returns:
//   |MOJO_RESULT_OK| if the trigger has been removed.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |trap_handle| is not a trap handle.
//   |MOJO_RESULT_NOT_FOUND| if there is no trigger registered on this trap for
//       the given value of |context|.
MOJO_SYSTEM_EXPORT MojoResult
MojoRemoveTrigger(MojoHandle trap_handle,
                  uintptr_t context,
                  const struct MojoRemoveTriggerOptions* options);

// Arms a trap, allowing it to invoke its event handler the next time any of its
// triggers' conditions are met.
//
// Parameters:
//   |trap_handle|: The handle of the trap to be armed.
//   |num_blocking_events|: An address pointing to the number of elements
//       available for storage at the address given by |blocking_events|.
//       Optional and only used when |MOJO_RESULT_FAILED_PRECONDITION| is
//       returned. See below.
//   |blocking_events|: An output buffer for |MojoTrapEvent| structures to be
//       filled in if one or more triggers would have tripped the trap
//       immediately if it were armed. Optional and used only when
//       |MOJO_RESULT_FAILED_PRECONDITION| is returned. See below.
//
// Returns:
//   |MOJO_RESULT_OK| if the trap has been successfully armed.
//       |num_blocking_events| and |blocking_events| are ignored.
//   |MOJO_RESULT_NOT_FOUND| if the trap does not have any triggers.
//       |num_blocking_events| and |blocking_events| are ignored.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |trap_handle| is not a valid trap handle,
//       or if |num_blocking_events| is non-null but |blocking_events| is
//       not.
//   |MOJO_RESULT_FAILED_PRECONDITION| if one or more triggers would have
//       tripped the trap immediately upon arming. If |num_blocking_events| is
//       non-null, this assumes there is enough space for |*num_blocking_events|
//       entries at the non-null address in |blocking_events|.
//
//       At most |*num_blocking_events| entries are populated there, with each
//       entry corresponding to one of the triggers which would have tripped the
//       trap. The actual number of entries populated is written to
//       |*num_blocking_events| before returning.
//
//       If there are more ready triggers than available provided storage, the
//       subset presented to the caller is arbitrary. The runtime makes an
//       effort to circulate triggers returned by consecutive failed
//       |MojoArmTrap()| calls so that callers may avoid handle starvation when
//       observing a large number of active handles with a single trap.
MOJO_SYSTEM_EXPORT MojoResult
MojoArmTrap(MojoHandle trap_handle,
            const struct MojoArmTrapOptions* options,
            uint32_t* num_blocking_events,
            struct MojoTrapEvent* blocking_events);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_TRAP_H_
