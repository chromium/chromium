# Linux Minidump Code Lab

[TOC]

## About Minidumps

Minidump is a file format for storing parts of a program's state for later
inspection. [Microsoft's
Documentation](https://docs.microsoft.com/en-us/windows/desktop/api/minidumpapiset/)
defines the format though the [Rust
Documentation](https://docs.rs/minidump/latest/minidump/format/index.html)
is sometimes easier to navigate. The minidump implementation and tools used by
Chrome are
[Breakpad](https://chromium.googlesource.com/breakpad/breakpad/+/refs/heads/main/README.md)
and
[Crashpad](https://chromium.googlesource.com/crashpad/crashpad/+/main/README.md).
However, the tools of interest here are from the Breakpad project.

## Create a Minidump

When Chrome crashes it writes out a minidump file. The minidump file is written
under the application product directory. On Linux this is
`<XDG_CONFIG_HOME>/<app-name>/Crash Reports`. The default for `XDG_CONFIG_HOME`
is `~.config`. Common `<app-name>`s are `chromium`, `google-chrome`,
`google-chrome-beta`, and `google-chrome-unstable`. A typical example is
`~.config/google-chrome/Crash Reports`. When a minidump is uploaded it will be
moved between the `new`, `pending`, and `completed` subdirectories. The minidump
file is named something like `<uuid>.dmp`. If the minidump is uploaded to the
crash reporting system, the `<uuid>.meta` file will contain the crash report id.
Those with access can find the uploaded report at `go/crash/<report-id>`, where
the minidump file will be available with a name like
`upload_file_minidump-<report-id>.dmp`.

To create a minidump, you can use a local build of Chromium or a release
version of Chrome. Run the browser with the environment variable
`CHROME_HEADLESS=1`, which enables crash dumping but prevents crash dumps from
being uploaded and deleted. Something like `$ env CHROME_HEADLESS=1
./out/debug/chrome-wrapper` or `$ env CHROME_HEADLESS=1
/opt/google/chrome/google-chrome`. Navigate to `chrome://crash` to trigger a
crash in the renderer process or reproduce your current crash bug. A crash dump
file should appear in the `Crash Reports` directory.

## Inspect the Minidump

To get an idea about what is in a minidump file, install the Okteta hex editor
and add the [Minidump Structure
Definition](https://github.com/bungeman/structures/tree/main/okteta-minidump).
Open the minidump previously created and explore the information it contains.

One quirk to notice is that there is a `ThreadListStream` which contains
`MINIDUMP_THREAD`s which contain a `MINIDUMP_THREAD_CONTEXT` and an
`ExceptionStream` which also contains a `MINIDUMP_THREAD_CONTEXT`. The thread
list contains the thread contexts as they existed when the crash reporter was
running. The exception's thread context is the state of the crashing thread at
the time that it crashed, which is generally the most interesting thread
context. When using the Breakpad tools for Linux (like `minidump_stackwalk` and
`minidump-2-core`) the thread context from the exception record is used in place
of the thread context associated with the corresponding thread.

Each `MINIDUMP_THREAD` contains a `StackMemoryRva` which is a reference to to a
copy of the stack on that thread at the time the crash handler was running.
Parsing a stack usefully requires additional debug information.
`minidump_stackwalk` or a debugger may be used to parse the stack memory to
create a usable trace.

## Get the Tools

From a Chromium checkout `ninja -C out/release minidump-2-core
minidump_stackwalk dump_syms`. From a [Breakpad
checkout](https://chromium.googlesource.com/breakpad/breakpad/) `make`. It can
be useful to use Breakpad directly on machines where one does not already have
a Chromium checkout.

When working at this level, one will also want to have `readelf` and
`objdump` available, which are available from most distributions.

## Get Executables and Symbols

In addition to the minidump, you will need the exact executables of Chromium or
Chrome which produced the minidump and those executable's symbols. If the
minidump was created locally, you already have the executables. Symbols for
Google Chrome's official builds are available from
`https://edgedl.me.gvt1.com/chrome/linux/symbols/google-chrome-debug-info-linux64-${VERSION}.zip`
where `${VERSION}` is any version of Google Chrome that has recently been served
to Stable, Beta, or Unstable (Dev) channels on Linux, like `114.0.5696.0`. Those
with access can find both executables and symbols for unreleased builds at
`go/chrome-symbols`.

For symbols outside of Chrome (like when the crash is happening in a shared
library) then symbols for the files of interest must be found. If the minidump
was created locally then install the symbol packages from your distribution.  If
not, you will need to track down the exact symbol files, which can be an
interesting exercise. For some distributions using the
[debuginfod](https://sourceware.org/elfutils/Debuginfod.html) system can be
quite helpful.

To ensure the correct binaries and debug symbols are used, the minidump contains
the build-id for each loaded module in the `ModuleListStream` in the
`CvRecordRva`'s `Signature`. This build-id is matched against a note
section of type `NT_GNU_BUILD_ID`, usually named `.note.gnu.build-id` in the
executable and symbol files. This note can be inspected with `readelf -n
<file>` like `readelf -n chrome` or `readelf -n chrome.debug` and looking for
the `.note.gnu.build-id` section. `readelf` reports the `Build ID` as the flat
bytes in the note, but Breakpad binaries like `stackwalk_minidimp` and
`dump_syms` will report and expect this truncated to a formatted Type 2 GUID
(without dashes). This means `readelf` will output a `<build-id>` like
33221100554477668899AABBCCDDEEFFXXXXXXXX but crashpad binaries will expect and
report this as a `<build-uuid>` of 00112233445566778899AABBCCDDEEFF.

The `.gnu_debuglink` section states which debug symbol file to use with a
striped binary. For example `readelf --string-dump=.gnu_debuglink chrome`
produces `chrome.debug`. This can be helpful to know for libraries with
interesting debug symbol setup, like libc.so.6.

## Create Symbolized Stack

Given a minidump with the name `mini.dmp`

`minidump_stackwalk mini.dmp > mini.stackwalk.nosym`

This will produce a mostly unsymbolized summary of the crash. To symbolize, look
toward the bottom of the output for `WARNING: No symbols, <file>, <build-uuid>`.
For each `<file>` which is of interest, `mkdir -p symbols/<file>/<build-uuid>` then
`dump_syms <file> <directory-with-file.debug> >
symbols/<file>/<build-uuid>/<file>.sym`. Ensure this output `<file>.sym`
contains the expected `<build-uuid>`. Then re-run `minidump_stackwalk` but
with the symbols directory, like `minidump_stackwalk mini.dmp symbols/ >
mini.stackwalk`.

The output of `minidump_stackwalk` is often quite useful and enough to track
down many issues. However, it does not fully use all of the information from
DWARF, so it is possible sometimes to get much better stack traces from a full
debugger like gdb. This is particularly true when functions have been
aggressively inlined.

## Create Core File

`minidump-2-core mini.dmp > mini.core`

## Loading into GDB

This works best if the binaries, symbols, and core files are all in different
directories to prevent gdb from automatically loading them into the wrong
locations. This is also generally necessary when using a system installed
version of Chrome. For full details see [the gdb
manual](https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html).
The easiest way is to rename and move the .debug files to a directory structure
like `<debugdir>/debug/.build-id/nn/nnnnnnnn.debug` where `nn` are the first
two hex characters of the `build-id`, and `nnnnnnnn` are the rest of the hex
characters of the `build-id`. Note that this `build-id` is exactly what is
reported by `readelf -n <binary> | grep "Build ID"` and not the `build-uuid`
used by Breakpad. Then in gdb use `show debug-file-directory` to get the
`<previous-directories>` and `set debug-file-directory
<previous-directories>:<debugdir>/debug`.

The `offset`s used here are the offsets of the corresponding module from the
output of `minidump_stackwalk` or (equivalently) the value of
`ModuleListStream::Modules[]::BaseOfImage` from the minidump file (which can be
read with the structure definition).

```
$ gdb
(gdb) file <executable>
(gdb) show debug-file-directory
<previous directories>
(gdb) set debug-file-directory <previous directories>:<debugdir>/debug
(gdb) symbol-file <executable> -o <executable-offset>
(gdb) core-file <mini.core>
```

Running the commands in this order avoids needing to load the symbols twice and
maps the `<executable>` to the expected location.

To add an additional shared library it is possible to
`(gdb) add-symbol-file <shared-library> -o <shared-library-offset>`

Source paths in Chrome builds are relative to the `out/<build>` directory. If you
have a Chromium checkout at or around when the Chrome build was created, it can
be added to the debugger search path, like

```
(gdb) directory <path-to-chromium>/chromium/src/out/<build>/
```

