# Guidance for //content/browser/renderer_host

For an overview of what this directory contains, see [README.md](README.md).
For the //content threat model and pitfalls that apply across //content, see
[//content/SECURITY.md](../../SECURITY.md).

This file is narrower than either of those. This is a short list of specific
security mistakes that reviewers have flagged repeatedly in code in this
directory. Read it before you change code here, whether you are a new
contributor or a coding agent.

When to add something here:
*   Add an item if it is specific to code in this directory and reviewers have
    had to flag it more than once.
*   If the pitfall applies outside this directory, add it to
    [//content/SECURITY.md](../../SECURITY.md) or the relevant doc in
    [docs/security](../../../docs/security/) instead, and link to it from here
    only if it needs a reminder.
*   Prefer linking to guidance that is documented elsewhere instead of restating
    it here.

## Security Pitfalls

### RenderProcessHost Teardown
*   Be wary of changing the lifetime of `RenderProcessHostImpl`, such as
    extending it during cleanup. Code across the browser assumes when an RPH
    is alive and when it is gone.
*   Do not clean up a `RenderProcessHostImpl` asynchronously. Do not post a task
    to call `Cleanup()` or to change the state that `Cleanup()` reads. An
    asynchronous cleanup lets other callers read the RPH state too late in its
    lifecycle.
*   All `RenderProcessHostImpl` state changes happen synchronously in
    `Cleanup()`. The only deferred UI-thread step is the final
    `DeleteSoon(this)`. It is deferred so that tasks already queued on the UI
    thread, such as IPC handlers that need the `RenderProcessHost` or its
    `ChildProcessSecurityPolicyImpl` state, can still run. Do not add other
    deferred steps.
*   Note: if `Cleanup()` is called from a
    `RenderProcessExited` observer, `ProcessDied()` defers it with
    `delayed_cleanup_needed_` and calls it again after all observers have
    run. This happens later in the same call stack, not in a posted task.
    It ensures that `RenderProcessHostDestroyed` is always the last observer
    callback.

### Re-entrancy
*   Calls to delegates, observers, Mojo callbacks, or the embedder can destroy
    the frame, widget, or `WebContents` you are using. See "Re-entrancy" in
    [Architectural Rules & Common API Pitfalls](../../SECURITY.md#architectural-rules-common-api-pitfalls).

### Renderer Trust
*   Do not trust anything that comes from the renderer. Code in this directory
    handles IPCs from renderers, and a compromised renderer can send any IPC
    at any time, with any value.
*   For info on how to validate renderer input and when to kill a misbehaving
    renderer, see:
    *   [Mojo Style Guide: Security](../../../docs/security/mojo.md#security)
        and
        [ReportBadMessage vs CHECK](../../../docs/security/mojo.md#reportbadmessage-vs-check)
    *   [Architectural Rules & Common API Pitfalls](../../SECURITY.md#architectural-rules-common-api-pitfalls)
    *   [Compromised Renderers](../../../docs/security/compromised-renderers.md)

### Navigation Initiators
*   The initiator frame can be gone by the time the browser handles a
    navigation. See: "Missing Navigation Initiators" in
    [Architectural Rules & Common API Pitfalls](../../SECURITY.md#architectural-rules-common-api-pitfalls).
