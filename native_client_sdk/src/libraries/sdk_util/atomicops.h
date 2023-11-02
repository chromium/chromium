/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_SDK_UTIL_ATOMICOPS_H_
#define LIBRARIES_SDK_UTIL_ATOMICOPS_H_

#ifndef WIN32

#include <stdint.h>

namespace sdk_util {

typedef int32_t Atomic32;

#ifndef __llvm__
static inline void MemoryBarrier() {
  __sync_synchronize();
}
#endif

inline Atomic32 AtomicCompareExchange(volatile Atomic32* ptr,
                                      Atomic32 new_value,
                                      Atomic32 old_value) {
  return __sync_val_compare_and_swap(ptr, new_value, old_value);
}

inline Atomic32 AtomicAddFetch(volatile Atomic32* ptr, Atomic32 value) {
  return __sync_add_and_fetch(ptr, value);
}

inline Atomic32 AtomicAndFetch(volatile Atomic32* ptr, Atomic32 value) {
  return __sync_and_and_fetch(ptr, value);
}

inline Atomic32 AtomicOrFetch(volatile Atomic32* ptr, Atomic32 value) {
  return __sync_or_and_fetch(ptr, value);
}

inline Atomic32 AtomicXorFetch(volatile Atomic32* ptr, Atomic32 value) {
  return __sync_xor_and_fetch(ptr, value);
}

}  // namespace sdk_util

#else  // ifndef WIN32

#include <windows.h>

/* Undefine many Windows.h macros that we almost certainly do not want. */
#undef min
#undef max
#undef PostMessage
#undef interface

namespace sdk_util {

typedef long Atomic32;

/* Windows.h already defines a MemoryBarrier macro. */

inline Atomic32 AtomicCompareExchange(volatile Atomic32* ptr,
                                      Atomic32 newvalue,
                                      Atomic32 oldvalue) {
  return InterlockedCompareExchange(ptr, newvalue, oldvalue);
}

inline Atomic32 AtomicAddFetch(volatile Atomic32* ptr, Atomic32 value) {
  return InterlockedExchangeAdd(ptr, value);
}

inline Atomic32 AtomicAndFetch(volatile Atomic32* ptr, Atomic32 value) {
  Atomic32 oldval;
  Atomic32 newval;
  do {
    oldval = *ptr;
    newval = oldval & value;
  } while (InterlockedCompareExchange(ptr, newval, oldval) != oldval);

  return newval;
}

inline Atomic32 AtomicOrFetch(volatile Atomic32* ptr, Atomic32 value) {
  Atomic32 oldval;
  Atomic32 newval;
  do {
    oldval = *ptr;
    newval = oldval | value;
  } while (InterlockedCompareExchange(ptr,newval, oldval) != oldval);

  return newval;
}

inline Atomic32 AtomicXorFetch(volatile Atomic32* ptr, Atomic32 value) {
  Atomic32 oldval;
  Atomic32 newval;
  do {
    oldval = *ptr;
    newval = oldval ^ value;
  } while (InterlockedCompareExchange(ptr,newval, oldval) != oldval);

  return newval;
}

}  // namespace sdk_util

#endif  // ifndef WIN32

#endif  // LIBRARIES_SDK_UTIL_ATOMICOPS_H_
