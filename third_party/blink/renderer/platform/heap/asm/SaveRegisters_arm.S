/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

.type PushAllRegisters, %function
.global PushAllRegisters
.hidden PushAllRegisters
#ifdef __thumb__
/* In THUMB Mode jump to ARM stub via bx to ensure CPU mode switch.
 * FIXME: This trampoline is provided to workaround bugs in
 * the THUMB/ARM interworking that appear in the component build.
 * When these issues are resolved this stub can be removed.
 */
.align 2
.code 16
.thumb_func
PushAllRegisters:
        adr r3, pushAllRegistersARM
        bx r3

.type pushAllRegistersARM, %function
.hidden pushAllRegistersARM
.align 4
.code 32
pushAllRegistersARM:
#else
/* ARM Mode */
.align 4
.code 32
PushAllRegisters:
#endif
        /* Push all callee-saved registers and save return address. */
        push {r4-r11, lr}
        /* Pass the first argument unchanged (r0)
         * and pass the stack pointer after pushing callee-saved
         * registers to the callback function.
         */
        mov r2, r1
        mov r1, sp
        blx r2
        /* Discard all the registers, and pop lr into pc which returns
         * and switches mode if needed.
         */
        add sp, sp, #32
        pop {pc}
