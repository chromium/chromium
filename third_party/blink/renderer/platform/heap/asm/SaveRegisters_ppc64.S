/*
 * typedef void (*PushAllRegistersCallback)(ThreadState*, intptr_t*);
 * extern "C" void PushAllRegisters(ThreadState*, PushAllRegistersCallback)
 */

.type PushAllRegisters, %function
.global PushAllRegisters
.hidden PushAllRegisters
PushAllRegisters:
        // Push all callee-saves registers to get them
        // on the stack for conservative stack scanning.
        // Reserve space for callee-saved registers and minimal stack frame.
        mflr 0
        std 0,16(1)
        stdu 1,-176(1) // 3218*8 = min stack  non-volatile registers

        // Save the callee-saved register
        std 31,168(1)
        std 30,160(1)
        std 29,152(1)
        std 28,144(1)
        std 27,136(1)
        std 26,128(1)
        std 25,120(1)
        std 24,112(1)
        std 23,104(1)
        std 22,96(1)
        std 21,88(1)
        std 20,80(1)
        std 19,72(1)
        std 18,64(1)
        std 17,56(1)
        std 16,48(1)
        std 15,40(1)
        std 14,32(1)
        // Note: the callee-saved floating point registers do not need to be
        // copied to the stack, because fp registers never hold heap pointers
        // and so do not need to be kept visible to the garbage collector.
        // Pass the first argument untouched in r3 and the
        // stack pointer to the callback.
        std 2, 24(1)
        mtctr 4
        mr 12, 4
        mr 4, 1
        bctrl
        ld 2, 24(1)

        // Adjust stack, restore return address and return.
        // Note: the copied registers do not need to be reloaded here,
        // because they were preserved by the called routine.
        addi 1,1,176
        ld 0,16(1)
        mtlr 0
        blr
