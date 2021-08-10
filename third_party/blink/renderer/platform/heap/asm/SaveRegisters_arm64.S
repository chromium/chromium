/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * typedef void (*PushAllRegistersCallback)(ThreadState*, intptr_t*);
 * extern "C" void PushAllRegisters(ThreadState*, PushAllRegistersCallback)
 */

/* Supplementary macro for setting function attributes */
.macro asm_function fname
#ifdef __APPLE__
    .globl _\fname
    .private_extern _\fname
_\fname:
#else
    .global \fname
#ifdef __ELF__
    .hidden \fname
    .type \fname, %function
#endif
\fname:
#endif
.endm

#if defined(__ARM_FEATURE_PAC_DEFAULT) && __ARM_FEATURE_PAC_DEFAULT
    // ENABLE_PAUTH must be defined to 1 since this value will be used in
    // bitwise-shift later!
    #define ENABLE_PAUTH 1

    #if ((__ARM_FEATURE_PAC_DEFAULT&((1<<0)|(1<<1)))==0)
        #error Pointer authentication defines no valid key!
    #endif
#else
    #define ENABLE_PAUTH 0
#endif

#if defined(__ARM_FEATURE_BTI_DEFAULT) && (__ARM_FEATURE_BTI_DEFAULT==1)
    // ENABLE_BTI must be defined to 1 since this value will be used in
    // bitwise-shift later!
    #define ENABLE_BTI 1
#else
    #define ENABLE_BTI 0
#endif


// Although Pointer Authentication and Branch Target Instructions are technically
// seperate features they work together, i.e. the paciasp and pacibsp instructions
// serve as BTI landing pads.
// Therefore PA-instructions are enabled when PA _or_ BTI is enabled!
#if ENABLE_PAUTH || ENABLE_BTI
    // See section "Pointer Authentication" of
    // https://developer.arm.com/documentation/101028/0012/5--Feature-test-macros
    // for details how to interpret __ARM_FEATURE_PAC_DEFAULT
    #if (__ARM_FEATURE_PAC_DEFAULT & (1<<0))
        #define PAUTH_SIGN_SP paciasp
        #define PAUTH_AUTH_SP autiasp
    #else
        #define PAUTH_SIGN_SP pacibsp
        #define PAUTH_AUTH_SP autibsp
    #endif
#else
    #define PAUTH_SIGN_SP
    #define PAUTH_AUTH_SP
#endif



asm_function PushAllRegisters
        .p2align 2

        PAUTH_SIGN_SP

        /* Save return address. */
        sub sp, sp, #96
        stp x19, x20, [sp, #80]
        stp x21, x22, [sp, #64]
        stp x23, x24, [sp, #48]
        stp x25, x26, [sp, #32]
        stp x27, x28, [sp, #16]
        stp x30, x29, [sp, #0]  // x30 is lr.

        /* Pass the first argument unchanged (x0)
         * and pass the stack pointer as third argument to the
         * callback function.
         */
        mov x2, x1
        mov x1, sp
        blr x2

        /* We don't restore all registers since they are callee saved (so
         * the callback didn't clobber them) and we didn't modify them either.
         */
        ldr x30, [sp], #96

        PAUTH_AUTH_SP

        ret

#if ENABLE_PAUTH || ENABLE_BTI
// see
// https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst#program-property
.pushsection .note.gnu.property, "a";
    .balign 8
    .long 4
    .long 0x10
    .long 0x5
    .asciz "GNU"
    .long 0xc0000000 /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
    .long 4
    .long ((ENABLE_PAUTH)<<1) | ((ENABLE_BTI)<<0) /* PAuth and BTI */
    .long 0
.popsection
#endif
