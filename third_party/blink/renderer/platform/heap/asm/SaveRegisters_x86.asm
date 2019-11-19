;; Copyright (C) 2013 Google Inc. All rights reserved.
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;
;;     * Redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above
;; copyright notice, this list of conditions and the following disclaimer
;; in the documentation and/or other materials provided with the
;; distribution.
;;     * Neither the name of Google Inc. nor the names of its
;; contributors may be used to endorse or promote products derived from
;; this software without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;

%ifndef X64POSIX
%define X64POSIX 0
%endif

%ifndef X64WIN
%define X64WIN 0
%endif

%ifndef IA32
%define IA32 0
%endif

%ifndef ARM
%define ARM 0
%endif

;; Prefix symbols by '_' if PREFIX is defined.
%ifdef PREFIX
%define mangle(x) _ %+ x
%else
%define mangle(x) x
%endif


; global_private makes a symbol a global but private to this shared library.
%ifidn   __OUTPUT_FORMAT__,elf32
  %define global_private(x) global mangle(x) %+ :function hidden
%elifidn __OUTPUT_FORMAT__,elf64
  %define global_private(x) global mangle(x) %+ :function hidden
%elifidn __OUTPUT_FORMAT__,elfx32
  %define global_private(x) global mangle(x) %+ :function hidden
%elifidn __OUTPUT_FORMAT__,macho32
  %define global_private(x) global mangle(x) %+ :private_extern
%elifidn __OUTPUT_FORMAT__,macho64
  %define global_private(x) global mangle(x) %+ :private_extern
%else
  %define global_private(x) global mangle(x)
%endif

section .text

;; typedef void (*PushAllRegistersCallback)(ThreadState*, intptr_t*);
;; extern "C" void PushAllRegisters(ThreadState*, PushAllRegistersCallback)

        global_private(PushAllRegisters)

%if X64POSIX

mangle(PushAllRegisters):
        ;; Push all callee-saves registers to get them
        ;; on the stack for conservative stack scanning.
        ;; We maintain 16-byte alignment at calls (required on Mac).
        ;; There is an 8-byte return address on the stack and we push
        ;; 56 bytes which maintains 16-byte stack alignment
        ;; at the call.
        push 0
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        ;; Pass the first argument unchanged (rdi)
        ;; and the stack pointer after pushing callee-saved
        ;; registers to the callback.
        mov r8, rsi
        mov rsi, rsp
        call r8
        ;; Pop the callee-saved registers. None of them were
        ;; modified so no restoring is needed.
        add rsp, 56
        ret

%elif X64WIN

mangle(PushAllRegisters):
        ;; Push all callee-saves registers to get them
        ;; on the stack for conservative stack scanning.
        ;; There is an 8-byte return address on the stack and we push
        ;; 72 bytes which maintains the required 16-byte stack alignment
        ;; at the call.
        push 0
        push rsi
        push rdi
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        ;; Pass the first argument unchanged (rcx)
        ;; and the stack pointer after pushing callee-saved
        ;; registers to the callback.
        mov r9, rdx
        mov rdx, rsp
        call r9
        ;; Pop the callee-saved registers. None of them were
        ;; modified so no restoring is needed.
        add rsp, 72
        ret

%elif IA32

mangle(PushAllRegisters):
        ;; Push all callee-saves registers to get them
        ;; on the stack for conservative stack scanning.
        ;; We maintain 16-byte alignment at calls (required on
        ;; Mac). There is a 4-byte return address on the stack
        ;; and we push 28 bytes which maintains 16-byte alignment
        ;; at the call.
        push ebx
        push ebp
        push esi
        push edi
        ;; Pass the first argument unchanged and the
        ;; stack pointer after pushing callee-save registers
        ;; to the callback.
        mov ecx, [esp + 24]
        push esp
        push dword [esp + 24]
        call ecx
        ;; Pop arguments and the callee-saved registers.
        ;; None of the callee-saved registers were modified
        ;; so we do not need to restore them.
        add esp, 24
        ret


%elif ARM
%error "NASM does not support arm. Use SaveRegisters_arm.S on arm."
%else
%error "Unsupported platform."
%endif
