# Linux Minidump to Core

On Linux, Chromium can use Breakpad to generate minidump files for crashes. It
is possible to convert the minidump files to core files, and examine the core
file in gdb, cgdb, or Qtcreator. In the examples below cgdb is assumed but any
gdb based debugger can be used.

[TOC]

## Creating the core file

Use `minidump-2-core` to convert the minidump file to a core file. On Linux, one
can build the minidump-2-core target in a Chromium checkout, or alternatively,
build it in a Google Breakpad checkout.

```shell
$ ninja -C out/Release minidump-2-core
$ ./out/Release/minidump-2-core foo.dmp > foo.core
```

## Retrieving Chrome binaries

If the minidump is from a public build then Googlers can find Google Chrome
Linux binaries and debugging symbols via https://goto.google.com/chromesymbols.
Otherwise, use the locally built chrome files. Google Chrome uses the
_debug link_ method to specify the debugging file. Either way be sure to put
`chrome` and `chrome.debug` (the stripped debug information) in the same
directory as the core file so that the debuggers can find them.

For Chrome OS release binaries look for `debug-*.tgz` files on
GoldenEye.

## Loading the core file into gdb/cgdb

The recommended syntax for loading a core file into gdb/cgdb is as follows,
specifying both the executable and the core file:

```shell
$ cgdb chrome foo.core
```

If the executable is not available then the core file can be loaded on its own
but debugging options will be limited:

```shell
$ cgdb -c foo.core
```

## Loading the core file into Qtcreator

Qtcreator is a full GUI wrapper for gdb and it can also load Chrome's core
files. From Qtcreator select the Debug menu, Start Debugging, Load Core File...
and then enter the paths to the core file and executable. Qtcreator has windows
to display the call stack, locals, registers, etc. For more information on
debugging with Qtcreator see
[Getting Started Debugging on Linux.](https://www.youtube.com/watch?v=xTmAknUbpB0)

## Source debugging

If you have a Chromium repo that is synchronized to exactly (or even
approximately) when the Chrome build was created then you can tell
`gdb/cgdb/Qtcreator` to load source code. Since all source paths in Chrome are
relative to the out/Release directory you just need to add that directory to
your debugger search path, by adding a line similar to this to `~/.gdbinit`:

```
(gdb) directory /usr/local/chromium/src/out/Release/
```

## Notes

*   Since the core file is created from a minidump, it is incomplete and the
    debugger may not know values for variables in memory. Minidump files contain
    thread stacks so local variables and function parameters should be
    available, subject to the limitations of optimized builds.
*   For gdb's `add-symbol-file` command to work, the file must have debugging
    symbols.
    *   In case of separate debug files,
    [the gdb manual](https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html)
    explains how gdb looks for them.
*   If the stack trace involve system libraries, the Advanced module loading
    steps shown below need to be repeated for each library.

## Advanced module loading

If gdb doesn't find shared objects that are needed you can force it to load
them. In gdb, the `add-symbol-file` command takes a filename and an address. To
figure out the address, look near the end of `foo.dmp`, which contains a copy of
`/proc/pid/maps` from the process that crashed.

One quick way to do this is with `grep`. For instance, if the executable is
`/path/to/chrome`, one can simply run:

```shell
$ grep -a /path/to/chrome$ foo.dmp

7fe749a90000-7fe74d28f000 r-xp 00000000 08:07 289158        /path/to/chrome
7fe74d290000-7fe74d4b7000 r--p 037ff000 08:07 289158        /path/to/chrome
7fe74d4b7000-7fe74d4e0000 rw-p 03a26000 08:07 289158        /path/to/chrome
```


In this case, `7fe749a90000` is the base address for `/path/to/chrome`, but gdb
takes the start address of the file's text section. To calculate this, one will
need a copy of `/path/to/chrome`, and run:

```shell
$ objdump -x /path/to/chrome | grep '\.text' | head -n 1 | tr -s ' ' | \
    cut -d' ' -f 7

005282c0
```


Now add the two addresses: `7fe749a90000 + 005282c0 = 7fe749fb82c0` and in gdb, run:

```
(gdb) add-symbol-file /path/to/chrome 0x7fe749fb82c0
```

Then use gdb as normal.

## Other resources

For more discussion on this process see
[Debugging a Minidump].
This page discusses the same process in the context of Chrome OS and many of the
concepts and techniques overlap.

[Debugging a Minidump](
https://www.chromium.org/chromium-os/packages/crash-reporting/debugging-a-minidump)
