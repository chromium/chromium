# Nested Message Loop Mechanism

## Overview
In Chromium, the UI thread's message pump processes work in a round-robin
fashion from two primary sources: the Chrome Task Queue and the native OS
Message Queue (e.g., the Windows Message Queue). Bugs may occur when the thread
blocks and spins a nested message loop, causing re-entrancy into UI code that
destroys objects currently in use by the blocked outer stack frames.

## Mechanisms of Re-entrancy
In the browser process, interactions with native Windows APIs and
accessibility tools often rely on COM (Component Object Model).

When the UI thread makes a synchronous cross-apartment COM call (or during
COM object teardown/release), the thread blocks. To allow for objects within
the apartment to respond, COM spins an internal message pump.
*   Unlike an explicit Chrome nested loop, this COM pump does **not**
    process Chrome tasks.
*   Unlike a standard Windows Message Pump, it does **not** process all
    application messages.
*   The nested COM message pump only pumps messages for its internal RPC window
    (`OleMainThreadWndClass`) within a restricted message range.
*   However, if a pending COM RPC callback is processed during this time,
    it can re-enter Chrome's codebase (e.g., through accessibility or input
    method event handlers), potentially destroying the `RenderWidgetHostView`
    or related structures while the original COM call is still on the stack.
