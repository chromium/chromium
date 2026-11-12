# Browser UI Thread Re-entrancy and Nested Message Loops

For the general `//content` security model, threat boundaries, and guidance on
evaluating observer/delegate bugs, read [content/SECURITY.md][content-sec] and
[Security for Agents][sec-agents] first. This document is the canonical
reference for how the browser UI thread's message pump and native OS calls
interact with re-entrancy across `//content/browser`, `//ui/views`, and
`//chrome/browser/ui`.

## Overview

On the browser UI thread, `base::MessagePumpForUI` coordinates two primary
sources of work:

1. **Chromium application tasks** managed by `base::SequenceManager` (including
   `PostTask` callbacks and incoming Mojo IPC dispatches), processed in
   time-bounded batches.
2. **Native OS events and messages** (such as the Windows message queue, macOS
   `CFRunLoop` / AppKit events, or Wayland/X11 events).

Re-entrancy bugs occur when code on the UI thread makes a call that either:

* synchronously invokes an in-process callback on the current call stack, or
* spins a nested message loop that dispatches work capable of mutating or
  destroying objects (such as `WebContents`, `RenderFrameHostImpl`,
  `RenderWidgetHostView`, `views::Widget`, or `views::View`) still referenced
  by outer stack frames.

## Chromium Tasks and Mojo IPCs Are Blocked by Default in Nested Loops

Whenever `SequenceManager` executes an application task (which includes any
`PostTask` callback or Mojo IPC handler) inside
`ThreadControllerWithMessagePumpImpl::DoWorkImpl()`, nestable task execution is
disabled (`task_execution_allowed = false`) for the duration of that task.

Consequently, while inside `DoWorkImpl()` (on **all platforms**):

* **Native OS nested loops entered from a task:** If code running inside a
  Chromium task or Mojo IPC handler calls a native OS API that spins an
  OS-level nested loop (for example, a Win32 modal loop, a COM STA call, an IME
  loop, a macOS `CFRunLoop`, or a GLib `GMainLoop`), **queued Chromium tasks
  and Mojo IPC messages do not execute** unless
  `base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop` is
  explicitly active on the stack.
* **Explicit `base::RunLoop`s entered from a task:** A `base::RunLoop` entered
  from inside a task does not run Chromium tasks or Mojo IPCs unless
  constructed with `base::RunLoop::Type::kNestableTasksAllowed`. Note that
  `kNestableTasksAllowed` only enables task execution for that `RunLoop::Run()`
  frame; `DoWorkImpl()` still resets `task_execution_allowed = false` while
  each task inside it runs, so a native OS loop entered from such a task still
  cannot run Chromium tasks without
  `ScopedAllowApplicationTasksInNativeNestedLoop`.
* **Outbound synchronous (`[Sync]`) Mojo calls:** Unlike nested message loops,
  making an outbound `[Sync]` Mojo IPC call blocks in Mojo's `SyncWatch` rather
  than `MessagePumpForUI`. Unless the method is marked `[NoInterrupt]` (or sync
  call interrupts are disabled), a thread waiting on an outbound `[Sync]` Mojo
  call can be interrupted to dispatch incoming `[Sync]` Mojo IPCs targeting
  that sequence. (Note that outbound `[Sync]` Mojo calls are disallowed by
  default in the browser process via `mojo::SyncCallRestrictions` and can only
  occur at call sites explicitly friend-listed with `ScopedAllowSyncCall`.)

When a nested loop is instead entered directly from a native OS event or
file-descriptor watch *outside* `DoWorkImpl()` (where `task_execution_allowed`
remains true), an explicit `base::RunLoop::Run()` call (such as `RunMoveLoop`
or Aura `MenuRunner` triggered from a mouse or key event) **does** run Chromium
tasks and Mojo IPCs on **all platforms** even with `RunLoop::Type::kDefault`
(because `ThreadControllerWithMessagePumpImpl::Run()` does not clear
`task_execution_allowed`). By contrast, when a **native OS** nested loop is
entered outside `DoWorkImpl()`, or when
`ScopedAllowApplicationTasksInNativeNestedLoop` is active, whether Chromium
tasks can run depends on the platform's message pump:

* **Windows (`MessagePumpForUI`):** `MessagePumpForUI::ScheduleWork()` normally
  wakes the pump via an internal event (`event_.Signal()`) and **only** posts
  its internal `kMsgHaveWork` message to Chrome's private message-only window
  (`message_window_`) while `ScopedAllowApplicationTasksInNativeNestedLoop` is
  active. Therefore, even if an unfiltered Win32 modal loop (`GetMessage` or
  `PeekMessage` with `hWnd == nullptr`, such as native menus, `DoDragDrop`, or
  common dialogs) is entered directly from a native OS event outside
  `DoWorkImpl()`, `kMsgHaveWork` is not posted and Chromium tasks do not run
  unless `ScopedAllowApplicationTasksInNativeNestedLoop` is active.
  Furthermore, `HWND`- or message-range-filtered loops (such as COM's internal
  STA wait loop) never dequeue `kMsgHaveWork` and never run Chromium tasks or
  Mojo IPCs even when that scoper is active.
* **macOS (`MessagePumpCFRunLoopBase`):** Chromium registers its work source
  (`work_source_`) in `kCFRunLoopCommonModes`. A nested `CFRunLoop` fires
  `work_source_` only if it runs in a common mode *and*
  `task_execution_allowed` is true (either because
  `ScopedAllowApplicationTasksInNativeNestedLoop` is active, or because the
  loop was entered directly from a native AppKit event outside `DoWorkImpl()`).
  Nested `CFRunLoop`s running in private or non-common run loop modes do not
  fire `work_source_`.
* **Linux (`MessagePumpGlib`):** Chromium attaches its recursive GLib work
  source (`work_source_`) to the default `GMainContext`. A nested GLib loop
  (`g_main_context_iteration` or `g_main_loop_run`) on the default context
  dispatches `work_source_` and runs Chromium tasks if and only if
  `task_execution_allowed` is true—meaning tasks do **not** run when the loop
  is entered from inside `DoWorkImpl()` without
  `ScopedAllowApplicationTasksInNativeNestedLoop`, but **can** run when the
  loop is entered directly from a native event or `FdWatch` callback outside
  `DoWorkImpl()`.

## Platform Mechanisms of Re-entrancy (and Common Misconceptions)

### 1. Synchronous In-Process Message Dispatch (No Message Pump Invoked)

* **Windows same-thread `SendMessage` and window APIs:** Calling Win32 APIs
  such as `SetWindowPos`, `ShowWindow`, `DestroyWindow`, `SetFocus`, or
  `SendMessage` on an `HWND` owned by the current thread synchronously invokes
  the target window procedure (`WndProc`) directly on the current call stack
  (delivering messages like `WM_WINDOWPOSCHANGED`, `WM_SIZE`, `WM_ACTIVATE`,
  `WM_KILLFOCUS`, or `WM_DESTROY`). Non-queued messages bypass the message
  queue and do **not** invoke Chrome's message pump, whereas Chromium tasks
  rely on queued/signaled pump wakeups and therefore cannot run during these
  calls.
* **macOS AppKit notifications and delegates:** Native AppKit calls (such as
  window ordering, focus/first-responder changes, or closing) can synchronously
  invoke `NSWindowDelegate`, `NSNotificationCenter` observers, or `NSView`
  methods on the current call stack without spinning a nested `CFRunLoop`. Do
  not assume a window or fullscreen transition spins a synchronous nested
  `CFRunLoop` without checking the implementation (for example,
  `NativeWidgetNSWindowFullscreenController::ExitFullscreen()` posts an
  asynchronous task rather than spinning a nested loop).

### 2. Windows Cross-Thread `SendMessage`

If the calling thread invokes `SendMessage` targeting an `HWND` owned by
*another* thread, the calling thread blocks until the target thread responds.
While blocked, Windows automatically dispatches incoming **non-queued**
`SendMessage` requests (`QS_SENDMESSAGE`) sent from other threads directly to
`WndProc`s on the waiting thread:

* As with same-thread calls, non-queued dispatch does not invoke Chrome's
  message pump and queued `PostMessage` traffic is not dispatched during this
  wait, so Chromium tasks and Mojo IPCs **cannot** execute.
* `SendMessage` calls do not occur without a caller. Any report citing a
  cross-thread `SendMessage` re-entrancy risk must identify **both**:
  1. the concrete **outbound** call on the UI thread that blocks and services
     `QS_SENDMESSAGE` (such as an outbound cross-thread `SendMessage` or COM
     STA wait), and
  2. the concrete **inbound** cross-thread `SendMessage` sent to a UI-thread
     `HWND` from another in-scope thread (see also the threat-model rules on
     third-party local processes below).

### 3. Windows COM Single-Threaded Apartment (STA) Calls

The browser UI thread initializes COM as a single-threaded apartment
(`COINIT_APARTMENTTHREADED`).

* **Outbound cross-apartment COM calls:** When an STA thread makes a
  synchronous cross-apartment COM call (or releases a cross-apartment proxy),
  COM blocks and enters an internal modal wait loop:
  * It never pumps `kMsgHaveWork` or general queued input/`PostMessage`
    traffic, so **Chromium tasks and Mojo IPCs never run**.
  * However, under COM's default STA message filter
    (`PENDINGMSG_WAITDEFPROCESS`), in addition to servicing COM RPC messages
    and incoming cross-thread `QS_SENDMESSAGE` dispatches, it can still
    dispatch certain window messages such as `WM_PAINT` and
    activation/task-switching messages on the thread's `HWND`s unless a
    scoped `IMessageFilter` overrides `MessagePending` to return
    `PENDINGMSG_WAITNOPROCESS` (see, for example,
    [crbug.com/458099545][crbug-458099545]).
  * Because Chrome does not register a global UI-thread `IMessageFilter` via
    `::CoRegisterMessageFilter` to reject incoming calls, COM's default STA
    wait loop accepts and dispatches both nested COM callbacks in the same
    causal chain (`CALLTYPE_NESTED`) and incoming
    `CALLTYPE_TOPLEVEL_CALLPENDING` COM calls while waiting on the outbound
    call. (Unrelated incoming COM calls from external local processes are out of
    scope; see below.)
* **In-process COM servers (`CLSCTX_INPROC_SERVER`):** Many COM objects used on
  the UI thread—such as Windows Text Services Framework (`TSFBridge` /
  `ITfThreadMgr`) and Direct Manipulation—are created in-process inside the UI
  thread's own STA. Calling a method on an in-process STA object is a direct
  virtual call rather than a cross-apartment RPC; however, the OS component's
  implementation can still internally send synchronous Win32 messages, make
  outbound cross-apartment COM calls, spin a modal loop, or synchronously
  invoke registered in-process sinks (such as `ITextStoreACP`).
* **Accessibility event APIs (`UiaRaiseAutomationEvent`, `NotifyWinEvent`):**
  * For **out-of-process** UI Automation subscribers,
    `::UiaRaiseAutomationEvent` and `::UiaRaiseAutomationPropertyChangedEvent`
    query any requested scope or cache properties synchronously in-process and
    write the event packet over a non-COM channel for asynchronous delivery in
    the client process without entering COM's STA modal wait loop
    ([crbug.com/575090745][crbug-575090745]). Only an **in-process** subscriber
    in another COM apartment (such as an injected DLL, in-process Windows Text
    Services TIP DLL, or the in-process MTA `IUIAutomation` test client in
    [crbug.com/583779800][crbug-583779800]) causes `uiautomationcore.dll` to
    unmarshal the handler from COM's Global Interface Table and make a blocking
    intra-process cross-apartment COM call that enters COM's STA modal wait
    loop. Even then, that COM wait loop **never** runs Chromium tasks or Mojo
    IPCs, though it can dispatch inbound UIA calls (such as
    `IInvokeProvider::Invoke` -> `AXPlatformNodeWin::Invoke`) before returning.
    Because untrusted web content and compromised renderers cannot inject DLLs
    or issue inbound UIA calls, re-entrancy via a local UIA client during a UIA
    raise is a **functional stability bug**, not a security vulnerability (see
    below).
  * `::NotifyWinEvent` is a Win32 (`user32.dll`) API rather than COM: it
    delivers out-of-process (`WINEVENT_OUTOFCONTEXT`) notifications
    asynchronously without blocking, entering COM's STA wait loop, or running
    Chromium tasks/IPCs, and only invokes `WINEVENT_INCONTEXT` hooks
    synchronously on the current stack if a local process has loaded a hook DLL
    into the browser process.

### 4. Explicit Nested Loops (`base::RunLoop` and Native Modal Loops)

Native OS modal loops (such as Win32 `TrackPopupMenu`, `DoDragDrop`, native
file/print dialogs, or AppKit modal sessions) and explicit
`base::RunLoop::Run()` calls pump native input and window events, which can
re-enter UI and `//content` code via user interaction or window lifecycle
events even when Chromium tasks and Mojo IPCs are blocked. See
[above][nested-loop-blocking] for when these loops also execute Chromium tasks
and Mojo IPCs.

## Threat Model and Validating Re-entrancy Reports

* **Cite the complete production call chain:** Per
  [content/SECURITY.md][content-sec-observers] and [Security for
  Agents][sec-agents], a valid report must identify the exact existing
  production call site that blocks or invokes a synchronous callback, the exact
  re-entrant path that mutates or destroys the object, and how an untrusted
  attacker (such as web content or a compromised renderer) can trigger both in
  sequence.
* **Local processes and injected DLLs are out of scope:** Other local desktop
  applications sending Win32 `SendMessage`s, driving COM or UI Automation
  clients, or loading in-process `WINEVENT_INCONTEXT` hook DLLs or third-party
  IMEs are already executing arbitrary unsandboxed code as the local user
  outside Chrome's security boundary. While crashes encountered by legitimate
  assistive technologies or IMEs may be fixed as **functional stability bugs**,
  an issue is only a **security vulnerability** if untrusted web content (or a
  compromised renderer) can trigger the memory corruption under normal user/OS
  operation without assuming a malicious local helper process or injected DLL.
* **Do not simulate native calls with `base::RunLoop`:** Per
  [content/SECURITY.md][content-sec-observers] and [Security for
  Agents][sec-agents-poc], modifying browser code to inject a `base::RunLoop`
  (especially with `kNestableTasksAllowed`), a synthetic `PostTask`, or a test
  observer/hook at a native OS or COM call site is **not** valid evidence that
  re-entrancy or task/Mojo execution can occur in production.

[content-sec]: /content/SECURITY.md
[content-sec-observers]: /content/SECURITY.md#hypothetical-observers
[sec-agents]: /docs/security/security-for-agents.md
[sec-agents-poc]: /docs/security/security-for-agents.md#escalated-code-execution
[nested-loop-blocking]: #chromium-tasks-and-mojo-ipcs-are-blocked-by-default-in-nested-loops
[crbug-458099545]: https://crbug.com/458099545
[crbug-575090745]: https://crbug.com/575090745
[crbug-583779800]: https://crbug.com/583779800
