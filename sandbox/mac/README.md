# The Mac Sandbox

The Mac sandbox provides an API for restricting the capabilities of a process.
The overall design philosophy of the sandbox is documented
[elsewhere](../docs/design/sandbox.md); this document explains the interface and
implementation of the Chromium sandbox on macOS.

## Seatbelt

On macOS, the sandbox uses the `sandbox(7)` API, sometimes referred to as
"Seatbelt". Note that this is different from the "App Sandbox", which is a
concept that applies to apps installed from the macOS App Store. The sandbox(7)
sandbox allows for a process to confine itself using a sandbox policy, which is
a declarative program describing what resources and APIs the process and its
children are allowed to use.

Those programs look like this:

```
(allow user-preference-read)
(allow file-data-read
  (path (user-homedir-path "/foo")))
(allow syscall-unix
  (syscall-number SYS_read)
  ...)
```

They are structured as a series of rules that either allow or deny access to
specific resources. The default stance for sandboxed processes is to give them
access only to resources they need access to, and so all our sandboxes use:

```
(deny default)
```

You can find the production sandbox policies in [../policy/mac](../policy/mac),
generally named after the process type they are applied to. [The "common"
policy](../policy/mac/common.sb) contains primitives and variables shared by all
process types.

Note that processes can still use any file descriptors they already had open,
even if the sandbox policy would deny them access to those resources now, so a
common pattern is to first open any needed resources, then apply the sandbox
policy.

## Writing A Sandbox Policy

Start with a default-deny sandbox, then run your new process type with
`--enable-sandbox-logging` (from common.sb), which will cause the sandbox to log
to `syslog(3)` whenever it denies a request. This will give you an empirical
idea of what resources your process needs to work. With that list in hand, you
will then need to assess whether:

* The process does need access to that resource, in which case you should grant
  it in your sandbox policy, or
* The process does need access to that resource but in a controlled way, in
  which case you should have it ask the browser process for the access it needs
  at runtime ("brokering"), or
* The process doesn't need access to it but is trying to use it anyway, in which
  case you should fix your code to not do that

Be especially careful if you're calling any Cocoa or AppKit APIs, since these
sometimes require access to certain resources to work. You should ensure you
have tested code paths leading to these very thoroughly **with the sandbox
enabled** - unit tests are not run in the sandbox!

Once you're all done, send your policy to a mamber of
[//sandbox/policy/mac/OWNERS](../policy/mac/OWNERS) for review.

## Applying The Sandbox

The `--process-type` argument to your subprocess declares what process type it
is, which dictates what type of sandbox it gets. The full list of types are in
[sandbox.mojom](../policy/mojom/sandbox.mojom).

Internally, the .sb policy files are compiled via `Seatbelt::Compile`, producing
a binary representation of the sandbox policy. This is passed over Mojo to
newly-created processes via `SeatbeltExecClient`; the newly-created process
receives it using `SeatbeltExecServer`, then applies it in
`SeatbeltExecServer::ApplySandboxProfile`.

## Debugging The Sandbox

See [//docs/mac/sandbox_debugging.md](../../docs/mac/sandbox_debugging.md).
