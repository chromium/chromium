# IPC Reviews

[TOC]

## What is IPC review and why is it needed?

A critical part of Chrome’s security is built on process isolation.
Untrustworthy content is isolated into unprivileged and sandboxed child
processes (e.g. renderer process, GPU process). In contrast, the browser
process is fully privileged and is not sandboxed, as it is considered
trustworthy.

In this model, the browser process acts as an operating system kernel for the
untrustworthy child processes. The interfaces defined by IPC are system calls
exposed to these (potentially compromised and) untrustworthy child processes,
allowing them to use system capabilities (such as the network) as appropriate.
Because IPCs cross trust and privilege boundaries between processes, it is
critical that IPCs themselves are robust against malicious input.

Consider the example interface below, assuming it is exposed to renderer
processes:

```
interface CookieJar {
  GetCookies(url.mojom.Url) => (string);
};
```

In normal circumstances, this isn’t problematic: a well-behaved renderer won’t
pass arbitrary URLs to `GetCookies()`; it will only pass URLs of Documents it’s
rendering. If the renderer is displaying email, it won’t need (or request) the
bank login cookies. However, an attacker with full control over the renderer
process can pass any URL to `GetCookies()`. An interface like this that
incorrectly trusts the renderer process would allow an attacker to exfiltrate
much more data than they should be able to.

IPC review is intended to catch these types of bugs. An adversary with full
control over an untrustworthy process must not be able to exploit IPC to escape
the sandboxed process. The goal is to ensure:

- IPC invariants are easy to maintain
- IPC interfaces are documented and understandable
- IPC handlers are resistant to malicious input

Note that IPC review is distinct from [security review for launches or major
changes to Chrome][chrome-security-review]; the latter is generally focused on
evaluating feature security at a high level, while IPC review is focused on
specific implementation details.

## What does an IPC reviewer look for?

- **What are the endpoints of the changed IPCs?** Almost all IPCs cross trust
  and privilege boundaries, so it is important to understand what processes are
  communicating, which way data flows, and if any of the endpoints are
  untrustworthy. This is often documented with interface-level comments, e.g.
  “the Widget interface is used by the browser process to inform the renderer
  process of UI state changes”.
- **What are the changes in capabilities being exposed over IPC?** Many changes
  provide new capabilities to an untrustworthy process. An IPC reviewer will
  evaluate if the new capabilities can be abused by an attacker (e.g. retrieve
  cookies of an unrelated page, write to arbitrary files, et cetera).
- **What breaks if an attacker provides malicious input?** If an array argument
  represents a point in 3D space, one assumption might be that it should contain
  exactly three elements. An IPC handler processing input from an untrustworthy
  process must not assume this though; it must validate that the sender actually
  provided exactly three elements. Note: in this case, an even better
  alternative would be define a `struct Point` with an `x`, `y`, and `z` fields:
  then it would be impossible for even a malicious sender to pass a malformed
  point!

To answer these questions, it’s often necessary to evaluate both the code
sending and the code reviewing the IPC. Avoid sending out changes where the IPC
handler is simply marked `NOTIMPLEMENTED()`: without an actual implementation,
it is often impossible to evaluate for potential security issues.

Please also keep in mind that an IPC reviewer often will not have all the
domain-specific knowledge in a given area (whether that be accessibility, GPU,
XR, or something else). Ensure that a change has appropriate context (in the CL
description and associated bugs), link to design documents, and thoroughly
document interfaces/methods/structs. Good documentation also helps future
readers of the code understand the system more easily.

## Guidelines and Best Practices for IPC

Please see the security team’s [Mojo guide][mojo-best-practices] for specific
guidelines and recommendations on how to structure IPC.

## When should IPC review happen?

In general, include an IPC reviewer when sending a change out for review. Even
if a change is under active development, it’s still OK to add an IPC reviewer.
While the IPC reviewer might not be actively involved if the design is still in
flux with other reviewers, simply being in the loop often provides useful
context for the change (and can sometimes save significant future pain).

It’s important to note that IPC review isn’t just a rubberstamp; as mentioned
above, an IPC reviewer’s focus is on reviewing cross-process interactions, from
the perspective of a hostile attacker trying to hijack a user’s machine. Adding
an IPC reviewer only after receiving all other LGTMs can sometimes be
frustrating for everyone involved, especially if significant revisions are
requested.

## Is it OK to TBR a simple change in IPC?

Avoid TBRing CLs with IPC changes. If the change is simple, ping an IPC reviewer
directly and ask them for a quick LGTM. Erring on the side of safety is
preferred for security-critical changes.

## IPC review is slow!

Please reach out to <ipc-security-reviewers@chromium.org> (for public issues)
or <chrome-security-ipc@google.com> (for internal issues). Large and complex
features can be difficult to evaluate on a change by change basis; reaching out
can help provide IPC reviewers with better context on the security properties
of the overall system, making it much easier to evaluate individual changes.

[chrome-security-review]: https://www.chromium.org/Home/chromium-security/security-reviews
[mojo-best-practices]: https://chromium.googlesource.com/chromium/src/+/master/docs/security/mojo.md
