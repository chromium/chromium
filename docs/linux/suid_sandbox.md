# Linux `SUID` Sandbox

*IMPORTANT NOTE: The Linux SUID sandbox is almost but not completely removed.
See https://bugs.chromium.org/p/chromium/issues/detail?id=598454
This page is mostly out-of-date.*

With [r20110](https://crrev.com/20110), Chromium on Linux can now sandbox its
renderers using a `SUID` helper binary. This is one of
[our layer-1 sandboxing solutions](sandboxing.md).

## `SUID` helper executable

The `SUID` helper binary is called `chrome_sandbox` and you must build it
separately from the main 'chrome' target. Chrome now just assumes it's next
to the executable in the same directory. You can also control its path
by CHROME_DEVEL_SANDBOX environment variable.

In order for the sandbox to be used, the following conditions must be met:

*   The sandbox binary must be executable by the Chromium process.
*   It must be `SUID` and executable by other.

If these conditions are met then the sandbox binary is used to launch the zygote
process. Once the zygote has started, it asks a helper process to chroot it to a
temp directory.

## `CLONE_NEWPID` method

The sandbox does three things to restrict the authority of a sandboxed process.
The `SUID` helper is responsible for the first two:

*   The `SUID` helper chroots the process. This takes away access to the
    filesystem namespace.
*   The `SUID` helper puts the process in a PID namespace using the
    `CLONE_NEWPID` option to
    [clone()](http://www.kernel.org/doc/man-pages/online/pages/man2/clone.2.html).
    This stops the sandboxed process from being able to `ptrace()` or `kill()`
    unsandboxed processes.

In addition:

*   The [Linux Zygote](zygote.md) startup code sets the process to be
    _undumpable_ using
    [prctl()](http://www.kernel.org/doc/man-pages/online/pages/man2/prctl.2.html).
    This stops sandboxed processes from being able to `ptrace()` each other.
    More specifically, it stops the sandboxed process from being `ptrace()`'d by
    any other process. This can be switched off with the
    `--allow-sandbox-debugging` option.

Limitations:

*   Not all kernel versions support `CLONE_NEWPID`. If the `SUID` helper is run
    on a kernel that does not support `CLONE_NEWPID`, it will ignore the problem
    without a warning, but the protection offered by the sandbox will be
    substantially reduced. See LinuxPidNamespaceSupport for how to test whether
    your system supports PID namespaces.
*   This does not restrict network access.
*   This does not prevent processes within a given sandbox from sending each
    other signals or killing each other.
*   Setting a process to be undumpable is not irreversible. A sandboxed process
    can make itself dumpable again, opening itself up to being taken over by
    another process (either unsandboxed or within the same sandbox).
    *   Breakpad (the crash reporting tool) makes use of this. If a process
        crashes, Breakpad makes it dumpable in order to use ptrace() to halt
        threads and capture the process's state at the time of the crash. This
        opens a small window of vulnerability.

## `setuid()` method

_This is an alternative to the `CLONE_NEWPID` method; it is not currently
implemented in the Chromium codebase._

Instead of using `CLONE_NEWPID`, the `SUID` helper can use `setuid()` to put the
process into a currently-unused UID, which is allocated out of a range of UIDs.
In order to ensure that the `UID` has not been allocated for another sandbox,
the `SUID` helper uses
[getrlimit()](http://www.kernel.org/doc/man-pages/online/pages/man2/getrlimit.2.html)
to set `RLIMIT_NPROC` temporarily to a soft limit of 1. (Note that the docs
specify that [setuid()](http://www.kernel.org/doc/man-pages/online/pages/man2/setuid.2.html)
returns `EAGAIN` if `RLIMIT_NPROC` is exceeded.)  We can reset `RLIMIT_NPROC`
afterwards in order to allow the sandboxed process to fork child processes.

As before, the `SUID` helper chroots the process.

As before, LinuxZygote can set itself to be undumpable to stop processes in the
sandbox from being able to `ptrace()` each other.

Limitations:

*   It is not possible for an unsandboxed process to `ptrace()` a sandboxed
    process because they run under different UIDs. This makes debugging harder.
    There is no equivalent of the `--allow-sandbox-debugging` other than turning
    the sandbox off with `--no-sandbox`.
*   The `SUID` helper can check that a `UID` is unused before it uses it (hence
    this is safe if the `SUID` helper is installed into multiple chroots), but
    it cannot prevent other root processes from putting processes into this
    `UID` after the sandbox has been started. This means we should make the
    `UID` range configurable, or distributions should reserve a `UID` range.

## `CLONE_NEWNET` method

The `SUID` helper uses
[CLONE_NEWNET](http://www.kernel.org/doc/man-pages/online/pages/man2/clone.2.html)
to restrict network access.

## Future work

We are splitting the `SUID` sandbox into a separate project which will support
both the `CLONE_NEWNS` and `setuid()` methods:
http://code.google.com/p/setuid-sandbox/

Having the `SUID` helper as a separate project should make it easier for
distributions to review and package.

## Possible extensions

## History

Older versions of the sandbox helper process will _only_ run
`/opt/google/chrome/chrome`. This string is hard coded
(`sandbox/linux/suid/sandbox.cc`). If your package is going to place the
Chromium binary somewhere else you need to modify this string.

## See also

*   [LinuxSUIDSandboxDevelopment](suid_sandbox_development.md)
*   [LinuxSandboxing](sandboxing.md)
*   General information on Chromium sandboxing:
    https://dev.chromium.org/developers/design-documents/sandbox
