// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of PreamblePatcher

#ifndef SANDBOX_WIN_SRC_SIDESTEP_PREAMBLE_PATCHER_H_
#define SANDBOX_WIN_SRC_SIDESTEP_PREAMBLE_PATCHER_H_

#include <stddef.h>

namespace sidestep {

// Maximum size of the preamble stub. We overwrite at least the first 5
// bytes of the function. Considering the worst case scenario, we need 4
// bytes + the max instruction size + 5 more bytes for our jump back to
// the original code. With that in mind, 32 is a good number :)
const size_t kMaxPreambleStubSize = 32;

// Possible results of patching/unpatching
enum SideStepError {
  SIDESTEP_SUCCESS = 0,
  SIDESTEP_INVALID_PARAMETER,
  SIDESTEP_INSUFFICIENT_BUFFER,
  SIDESTEP_JUMP_INSTRUCTION,
  SIDESTEP_FUNCTION_TOO_SMALL,
  SIDESTEP_UNSUPPORTED_INSTRUCTION,
  SIDESTEP_NO_SUCH_MODULE,
  SIDESTEP_NO_SUCH_FUNCTION,
  SIDESTEP_ACCESS_DENIED,
  SIDESTEP_UNEXPECTED,
};

// Implements a patching mechanism that overwrites the first few bytes of
// a function preamble with a jump to our hook function, which is then
// able to call the original function via a specially-made preamble-stub
// that imitates the action of the original preamble.
//
// Note that there are a number of ways that this method of patching can
// fail.  The most common are:
//    - If there is a jump (jxx) instruction in the first 5 bytes of
//    the function being patched, we cannot patch it because in the
//    current implementation we do not know how to rewrite relative
//    jumps after relocating them to the preamble-stub.  Note that
//    if you really really need to patch a function like this, it
//    would be possible to add this functionality (but at some cost).
//    - If there is a return (ret) instruction in the first 5 bytes
//    we cannot patch the function because it may not be long enough
//    for the jmp instruction we use to inject our patch.
//    - If there is another thread currently executing within the bytes
//    that are copied to the preamble stub, it will crash in an undefined
//    way.
//
// If you get any other error than the above, you're either pointing the
// patcher at an invalid instruction (e.g. into the middle of a multi-
// byte instruction, or not at memory containing executable instructions)
// or, there may be a bug in the disassembler we use to find
// instruction boundaries.
class PreamblePatcher {
 public:
  // Patches target_function to point to replacement_function using a provided
  // preamble_stub of stub_size bytes.
  // Returns An error code indicating the result of patching.
  template <class T>
  static SideStepError Patch(T target_function,
                             T replacement_function,
                             void* preamble_stub,
                             size_t stub_size) {
    return RawPatchWithStub(target_function, replacement_function,
                            reinterpret_cast<unsigned char*>(preamble_stub),
                            stub_size, nullptr);
  }

 private:
  // Patches a function by overwriting its first few bytes with
  // a jump to a different function.  This is similar to the RawPatch
  // function except that it uses the stub allocated by the caller
  // instead of allocating it.
  //
  // To use this function, you first have to call VirtualProtect to make the
  // target function writable at least for the duration of the call.
  //
  // target_function: A pointer to the function that should be
  // patched.
  //
  // replacement_function: A pointer to the function that should
  // replace the target function.  The replacement function must have
  // exactly the same calling convention and parameters as the original
  // function.
  //
  // preamble_stub: A pointer to a buffer where the preamble stub
  // should be copied. The size of the buffer should be sufficient to
  // hold the preamble bytes.
  //
  // stub_size: Size in bytes of the buffer allocated for the
  // preamble_stub
  //
  // bytes_needed: Pointer to a variable that receives the minimum
  // number of bytes required for the stub.  Can be set to nullptr if you're
  // not interested.
  //
  // Returns An error code indicating the result of patching.
  static SideStepError RawPatchWithStub(void* target_function,
                                        void* replacement_function,
                                        unsigned char* preamble_stub,
                                        size_t stub_size,
                                        size_t* bytes_needed);
};

}  // namespace sidestep

#endif  // SANDBOX_WIN_SRC_SIDESTEP_PREAMBLE_PATCHER_H_
