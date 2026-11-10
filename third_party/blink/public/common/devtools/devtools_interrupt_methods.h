// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVTOOLS_INTERRUPT_METHODS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVTOOLS_INTERRUPT_METHODS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace blink {

// Allow-list of Chrome DevTools Protocol methods that are safe to dispatch by
// interrupting V8 execution on the target thread (and that should be sent over
// the IO session for frames so they can be serviced while the main thread is
// paused or busy).
//
// SECURITY NOTE: V8 interrupt callbacks run at stack check points (such as
// optimized loop back-edges) that JIT compilers model as having no JS heap
// writes. Only methods proven not to execute arbitrary page JavaScript (no
// getters, Proxy traps, Error.prepareStackTrace, toString/valueOf, or
// microtasks) may be listed here.
inline constexpr auto kDevToolsInterruptMethods =
    base::MakeFixedFlatSet<std::string_view>({
        "Debugger.getPossibleBreakpoints",
        "Debugger.getScriptSource",
        "Debugger.getStackTrace",
        "Debugger.pause",
        "Debugger.removeBreakpoint",
        "Debugger.resume",
        "Debugger.setBreakpoint",
        "Debugger.setBreakpointByUrl",
        "Debugger.setBreakpointsActive",
        "Emulation.setScriptExecutionDisabled",
        "Page.crash",
        "Performance.getMetrics",
        "Runtime.terminateExecution",
    });

constexpr bool ShouldInterruptForMethod(std::string_view method) {
  return kDevToolsInterruptMethods.contains(method);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DEVTOOLS_DEVTOOLS_INTERRUPT_METHODS_H_
