# seccomp Sandbox Crash Dumping

Currently, Breakpad relies on facilities that are disallowed inside the Linux
seccomp sandbox.  Specifically, it sets a signal handler to catch faults
(currently disallowed), forks a new process, and uses ptrace() (also disallowed)
to read the memory of the faulted process.

## Options

There are three ways we could do crash dumping of seccomp-sandboxed processes:

*   Find a way to permit signal handling safely inside the sandbox (see below).
*   Allow the kernel's core dumper to kick in and write a core file.
    *   This seems risky because this code tends not to be well-tested.
    *   This will not work if the process is chrooted, so it would not work if
        the seccomp sandbox is stacked with the SUID sandbox.
*   Have an unsandboxed helper process which `ptrace()`s the sandboxed process
    to catch faults.

## Signal handling in the seccomp sandbox

In case a trusted thread faults with a SIGSEGV, we must make sure that an
untrusted thread cannot register a signal handler that will run in the context
of the trusted thread.

Here are some mechanisms that could make this safe:

*   `sigaltstack()` is per-thread. If we opt not to set a signal stack for
    trusted threads, and set %esp/%rsp to an invalid address, trusted threads
    will die safely if they fault.
    *   This means the trusted thread cannot set a signal stack on behalf of the
        untrusted thread once the latter has switched to seccomp mode. The
        signal stack would have to be set up when the thread is created and not
        subsequently changed.
*   `clone()` has a `CLONE_SIGHAND` flag. By omitting this flag, trusted and
    untrusted threads can have different sets of signal handlers. This means we
    can opt not to set signal handlers for trusted threads.
    *   Again, per-thread signal handler sets would mean the trusted thread
        cannot change signal handlers on behalf of untrusted threads.
*   `sigprocmask()/pthread_sigmask()`: These can be used to block signal
    handling in trusted threads.

## See also

*   [LinuxCrashDumping](linux/crash_dumping.md)
*   [Issue 37728](https://crbug.com/37728)
