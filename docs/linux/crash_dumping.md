# Linux Crash Dumping

Official builds of Chrome support crash dumping and reporting using the Google
crash servers. This is a guide to how this works.

[TOC]

## Breakpad

Breakpad is an open source library which we use for crash reporting across all
three platforms (Linux, Mac and Windows). For Linux, a substantial amount of
work was required to support cross-process dumping. At the time of writing this
code is currently forked from the upstream breakpad repo. While this situation
remains, the forked code lives in `third_party/breakpad/linux`. The upstream
repo is mirrored in `third_party/breakpad/breakpad`.

The code currently supports i386 only. Getting x86-64 to work should only be a
minor amount of work.

### Minidumps

Breakpad deals in a file format called 'minidumps'. This is a Microsoft format
and thus is defined by in-memory structures which are dumped, raw, to disk. The
main header file for this file format is
`third_party/breakpad/breakpad/src/google_breakpad/common/minidump_format.h`.

At the top level, the minidump file format is a list of key-value pairs. Many of
the keys are defined by the minidump format and contain cross-platform
representations of stacks, threads etc. For Linux we also define a number of
custom keys containing `/proc/cpuinfo`, `lsb-release` etc. These are defined in
`third_party/breakpad/breakpad/linux/minidump_format_linux.h`.

### Catching exceptions

Exceptional conditions (such as invalid memory references, floating point
exceptions, etc) are signaled by synchronous signals to the thread which caused
them. Synchronous signals are always run on the thread which triggered them as
opposed to asynchronous signals which can be handled by any thread in a
thread-group which hasn't masked that signal.

All the signals that we wish to catch are synchronous except SIGABRT, and we can
always arrange to send SIGABRT to a specific thread. Thus, we find the crashing
thread by looking at the current thread in the signal handler.

The signal handlers run on a pre-allocated stack in case the crash was triggered
by a stack overflow.

Once we have started handling the signal, we have to assume that the address
space is compromised. In order not to fall prey to this and crash (again) in the
crash handler, we observe some rules:

1.  We don't enter the dynamic linker. This, observably, can trigger crashes in
    the crash handler. Unfortunately, entering the dynamic linker is very easy
    and can be triggered by calling a function from a shared library who's
    resolution hasn't been cached yet. Since we can't know which functions have
    been cached we avoid calling any of these functions with one exception:
    `memcpy`. Since the compiler can emit calls to `memcpy` we can't really
    avoid it.
1.  We don't allocate memory via malloc as the heap may be corrupt. Instead we
    use a custom allocator (in `breakpad/linux/memory.h`) which gets clean pages
    directly from the kernel.

In order to avoid calling into libc we have a couple of header files which wrap
the system calls (`linux_syscall_support.h`) and reimplement a tiny subset of
libc (`linux_libc_support.h`).

### Self dumping

The simple case occurs when the browser process crashes. Here we catch the
signal and `clone` a new process to perform the dumping. We have to use a new
process because a process cannot ptrace itself.

The dumping process then ptrace attaches to all the threads in the crashed
process and writes out a minidump to `/tmp`. This is generic breakpad code.

Then we reach the Chrome specific parts in `chrome/app/breakpad_linux.cc`. Here
we construct another temporary file and write a MIME wrapping of the crash dump
ready for uploading. We then fork off `wget` to upload the file. Based on Debian
popcorn, `wget` is very commonly installed (much more so than `libcurl`) and
`wget` handles the HTTPS gubbins for us.

### Renderer dumping

In the case of a crash in the renderer, we don't want the renderer handling the
crash dumping itself. In the future we will sandbox the renderer and allowing it
the authority to crash dump itself is too much.

Thus, we split the crash dumping in two parts: the gathering of information
which is done in process and the external dumping which is done out of process.
In the case above, the latter half was done in a `clone`d child. In this case,
the browser process handles it.

When renderers are forked off, they have a `UNIX DGRAM` socket in file
descriptor 4. The signal handler then calls into Chrome specific code
(`chrome/renderer/render_crash_handler_linux.cc`) when it would otherwise
`clone`. The Chrome specific code sends a datagram to the socket which contains:

*   Information which is only available to the signal handler (such as the
    `ucontext` structure).
*   A file descriptor to a pipe which it then blocks on reading from.
*   A `CREDENTIALS` structure giving its PID.

The kernel enforces that the renderer isn't lying in the `CREDENTIALS` structure
so it can't ask the browser to crash dump another process.

The browser then performs the ptrace and minidump writing which would otherwise
be performed in the `clone`d process and does the MIME wrapping the uploading as
normal.

Once the browser has finished getting information from the crashed renderer via
ptrace, it writes a byte to the file descriptor which was passed from the
renderer. The renderer than wakes up (because it was blocking on reading from
the other end) and rethrows the signal to itself. It then appears to crash
'normally' and other parts of the browser notice the abnormal termination and
display the sad tab.

## How to test Breakpad support in Chromium

*   Build Chromium as normal.
*   Run the browser with the environment variable
    [CHROME_HEADLESS=1](https://crbug.com/19663). This enables crash dumping but
    prevents crash dumps from being uploaded and deleted.

    ```shell
    env CHROME_HEADLESS=1 ./out/Debug/chrome-wrapper
    ```
*   Visit the special URL `chrome://crash` to trigger a crash in the renderer
    process.
*   A crash dump file should appear in the directory
    `~/.config/chromium/Crash Reports`.
