// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_nacl_irt_stack_alignment.h"

#include <stddef.h>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

// This whole test is really only meant for x86-32 NaCl (not PNaCl).
//
// This is a regression test for the IRT code being sensitive to stack
// alignment.  The de jure ABI is that the stack should be aligned to
// 16 bytes at call sites.  However, the de facto ABI is that the IRT
// worked in the past when called with misaligned stack.  NaCl code is
// now compiled to expect the proper 16-byte alignment, but the IRT
// code must remain compatible with old binaries that failed to do so.

#if defined(__i386__)

REGISTER_TEST_CASE(NaClIRTStackAlignment);

bool TestNaClIRTStackAlignment::Init() {
  var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  return var_interface_ && CheckTestingInterface();
}

void TestNaClIRTStackAlignment::RunTests(const std::string& filter) {
  RUN_TEST(MisalignedCallVarAddRef, filter);
}

// This calls the given function with the stack explicitly misaligned.
// If the function (in the IRT) was compiled wrongly, it will crash.
void MisalignedCall(void (*func)(PP_Var), const PP_Var* arg)
    asm("MisalignedCall") __attribute__((regparm(2)));

// regparm(2) means: First argument in %eax, second argument in %edx.
// Writing this with an inline asm would require explaining all the
// call-clobbers register behavior in the asm clobber list, which is a
// lot with all the SSE and FPU state.  It's far simpler just to make
// it a function call the compiler knows is a function call, and then
// write the function itself in pure assembly.
asm("MisalignedCall:\n"
    // Use an SSE register to copy the 16 bytes of memory.
    // Note this instruction does not care about alignment.
    // The pointer is not necessarily aligned to 16 bytes.
    "movups (%edx), %xmm0\n"
    // Set up a frame so we can recover the stack pointer after alignment.
    "push %ebp\n"
    "mov %esp, %ebp\n"
    // Align the stack properly to 16 bytes.
    "andl $-16, %esp\n"
    // Now make space for the 16 bytes of argument data,
    // plus another 4 bytes so the stack pointer is misaligned.
    "subl $20, %esp\n"
    // Copy the argument onto the (misaligned) top of stack.
    "movups %xmm0, (%esp)\n"
    // Now call into the IRT, and hilarity ensues.
    "naclcall %eax\n"
    // Standard epilogue.
    "mov %ebp, %esp\n"
    "pop %ebp\n"
    "naclret");

std::string TestNaClIRTStackAlignment::TestMisalignedCallVarAddRef() {
  PP_Var var;
  var.type = PP_VARTYPE_INT32;
  var.padding = 0;
  var.value.as_int = 23;

  ASSERT_EQ(sizeof(var), static_cast<size_t>(16));

  // This will crash if the test fails.
  MisalignedCall(var_interface_->AddRef, &var);
  MisalignedCall(var_interface_->Release, &var);

  PASS();
}

#endif  // defined(__i386__)
