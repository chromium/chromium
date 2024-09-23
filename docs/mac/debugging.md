# Debugging Chromium on macOS

[TOC]

## Debug vs. Release Builds

Debug builds are the default configuration for Chromium and can be explicitly
specified with `is_debug=true` in the `args.gn` file of the out directory. Debug
builds are larger and non-portable because they default to
`is_component_build=true`, but they contain full debug information.

If you set `is_debug=false`, a release build will be created with no symbol
information, which cannot be used for effective debugging.

A middle-ground is to set `symbol_level=1`, which will produce a minimal symbol
table, capable of creating backtraces, but without frame-level local variables.
This is faster to build than a debug build, but it is less useful for debugging.

When doing an `is_official_build=true` build (which is meant for producing
builds with full compiler optimization suitable for shipping to users),
`enable_dsyms` and `enable_stripping` both get set to `true`. The binary itself
will be stripped of its symbols, but the debug information will be saved off
into a dSYM file. Producing a dSYM is rather slow, so it is uncommon for
developers to build with this configuration.

### Chrome Builds

The official Google Chrome build has published dSYMs that can be downloaded with
the script at `tools/mac/download_symbols.py` or by using the LLDB integration
at `tools/lldb/lldb_chrome_symbols.py`.

However, the official Chrome build is
[codesigned](../../chrome/installer/mac/signing/README.md) with the `restrict`
and `runtime` options, which generally prohibit debuggers from attaching.

In order to debug production/released Chrome, you need to do one of two things:

1. Disable [System Integrity
Protection](https://developer.apple.com/documentation/security/disabling_and_enabling_system_integrity_protection),
by:
    1. Rebooting into macOS recovery mode
    2. Launching Terminal
    3. Running `csrutil enable --without debug`
    4. Rebooting
2. Stripping or force-re-codesigning the binary to not use those options:
   `codesign --force --sign - path/to/Google\ Chrome.app`

If you will frequently debug official builds, (1) is recommended. Note that
disabling SIP reduces the overall security of the system, so your system
administrator may frown upon it.

## The Debugger

The debugger on macOS is `lldb` and it is included in both a full Xcode install
and the Command Line Tools package. There are two ways to use LLDB: either
launching Chromium directly in LLDB, or attaching to an existing process:

    lldb ./out/debug/Chromium.app/Contents/MacOS/Chromium
    lldb -p <pid>

LLDB has an extensive help system which you can access by typing `help` at the
`(lldb)` command prompt. The commands are organized into a functional hierarchy,
and you can explore the subcommands via `(lldb) help breakpoint`, etc. Commands
can take arguments in a command-line flag style. Many commands also have short
mnemonics that match the `gdb` equivalents. You can also just use enough letters
to form a unique prefix of the command hierarchy.  E.g., these are equivalent:

    (lldb) help breakpoint set
    (lldb) h br s

When the program is running, you can use **Ctrl-C** to interrupt it and pause
the debugger.

### Passing Arguments

To pass arguments to LLDB when starting Chromium, use a `--`:

    lldb ./out/debug/Chromium.app/contents/MacOS/Chromium -- --renderer-startup-dialog

### Breakpoints

Simple function-name breakpoints can be specified with a short mnemonic:

    (lldb) b BrowserWindow::Close

But there are a range of other options for setting breakpoints using the, such
as:
* `-t` to limit the breakpoint to a specific thread
* `-s` to specify a specific shared library, if the same symbol name is exported
  by multiple libraries
* `-o` for a one-shot breakpoint (delete after first hit)

See `(lldb) help br set` for full details.

### Navigating the Stack

When the debugger is paused, you can get a backtrace by typing `bt`. To navigate
the stack by-1 type either `up` or `down`. You can also jump to a specific index
in the stack by typing `f #` (short for `frame select #`).

To see local variables, type `v` (short for `frame variable`).

### Examining Execution

To step through the program, use:

* `s` or `si` for step-in
* `n` for step-over
* `c` to continue (resume or go to next breakpoint)

### Printing Values

To print values, use the `p <value>` command, where `<value>` can be a variable,
a variable expression like `object->member_->sub_member_.value`, or an address.

If `<value>` is a pointer to a structure, `p <value>` will usually just print
the address. To show the contents of the structure, dereference the value. E.g.:

    (lldb) p item
    (HistoryMenuBridge::HistoryItem *) $3 = 0x0000000245ef5b30
    (lldb) p *item
    (HistoryMenuBridge::HistoryItem) $4 = {
      title = u"Google"
      url = {
        spec_ = "https://www.google.com/"
        is_valid_ = true
    …

Note above that LLDB has also stored the results of the expressions passed to
`p` into the variables `$3` and `$4`, which can be referenced in other LLDB
expressions.

Often (and always when printing addresses) there is not type information to
enable printing the full structure of the referenced memory. In these cases, use
a C-style cast:

    (lldb) p 0x0000000245ef5b30  # Does not have type information
    (long) $5 = 9763248944
    (lldb) p (HistoryMenuBridge::HistoryItem*)0x0000000245ef5b30
    (HistoryMenuBridge::HistoryItem *) $6 = 0x0000000245ef5b30
    (lldb) p *$6
    (HistoryMenuBridge::HistoryItem) $7 = {
      title = u"Google"
    …

* For printing Cocoa NSObjects, use the `po` command to get the `-[NSObject description]`.
* If `uptr` is a `std::unique_ptr`, the address it wraps is accessible as
  `uptr.__ptr_.__value_`.
* To pretty-print `std::u16string`, follow the instructions [here](../lldbinit.md).

## Multi-Process Debugging

Chrome is split into multiple processes, which can mean that the logic you want
to debug is in a different process than the main browser/GUI process. There are
a few ways to debug the multi-process architecture, discussed below.

### (a) Attach to a Running Process

You can use Chrome's Task Manager to associate specific sites with their PID.
Then simply attach with LLDB:

    lldb -p <pid>

Or, if you have already been debugging a Chrome process and want to retain your
breakpoints:

    (lldb) attach <pid>

### (b) Debug Process Startup

If you need to attach early in the child process's lifetime, you can use one of
these startup-dialog switches for the relevant process type:

* `--renderer-startup-dialog`
* `--utility-startup-dialog`
* `--utility-startup-dialog=data_decoder.mojom.DataDecoderService`

After the process launches, it will print a message like this to standard error:

    [80156:775:0414/130021.862239:ERROR:content_switches_internal.cc(112)] Renderer (80156) paused waiting for debugger to attach. Send SIGUSR1 to unpause.

Then attach the the process like above in **(a)**, using the PID in parenthesis
(e.g. *80156* above).

### (c) Run Chrome in a single process

> This option is not recommended. Single-process mode is not tested and is
> frequently broken.

Chrome has an option to run all child processes as threads inside a single
process, using the `--single-process` command line flag. This can make debugging
easier.

## Debugging Out-of-Process Tests:

Similar to debugging the renderer process, simply attaching LLDB to a
out-of-process test like browser\_tests will not hit the test code. In order to
debug a browser test, you need to run the test binary with  "`--single_process`"
(note the underscore in `single_process`). Because you can only run one browser
test in the same process, you're probably going to need to add `--gtest_filter`
as well. So your command will look like this:

    /path/to/src/out/debug/browser_tests --single_process --gtest_filter=GoatTeleporterTest.DontTeleportSheep

## Working with Xcode

If you'd prefer to use Xcode GUI to use the debugger, there are two options:

### (1) Empty Xcode Project

This approach creates an empty Xcode project that only provides a GUI debugger:

1. Select **File** > **New** > **Project...** and make a new project. Dump it
   anywhere, call it anything. It doesn't matter.
2. Launch Chromium.
3. In Xcode, select **Debug** > **Attach to Process** > *Chromium*.
4. You can now pause the process and set breakpoints. The debugger will also
   activate if a crash occurs.

### (2) Use *gn*

1. Tell `gn` to generate an Xcode project for your out directory:
   `gn gen --ide=xcode out/debug`
2. Open *out/debug/all.xcodeproj*
3. Have it automatically generate schemes for you
4. You can now build targets from within Xcode, which will simply call out to
   `ninja` via an Xcode script. But the resulting binaries are available as
   debuggable targets in Xcode.

Note that any changes to the .xcodeproj will be overwritten; all changes to the
build system need to be done in GN.

## Debugging the Sandbox

See the page on [sandbox debugging](sandbox_debugging.md).

## System Permission Prompts; Transparency, Consent, and Control (TCC)

When debugging issues with OS-mediated permissions (e.g. Location, Camera,
etc.), you need to launch Chromium with LaunchServices rather than through a
shell. If you launch Chromium as a subprocess of your terminal shell, the
permission requests get attributed to the terminal app rather than Chromium.

To launch Chromium via launch services, use the `open(1)` command:

    open ./out/debug/Chromium.app

To pass command line arguments:

    open ./out/debug/Chromium.app -- --enable-features=MyCoolFeature

## Taking CPU Samples

A quick and easy way to investigate slow or hung processes is to use the sample
facility, which will generate a CPU sample trace. This can be done either in the
Terminal with the sample(1) command or by using Activity Monitor:

1. Open Activity Monitor
2. Find the process you want to sample (for "Helper" processes, you may want to
   consult the Chrome Task Manager)
3. Double-click on the row
4. Click the **Sample** button in the process's information window

After a few seconds, the sample will be completed. For official Google Chrome
builds, the sample should be symbolized using
[crsym](https://goto.google.com/crsym/). If you do not have access to crsym,
save the *entire* contents as a file and attach it to a bug report for later
analysis.

See also [How to Obtain a Heap
Dump](../memory-infra/heap_profiler.md#how-to-obtain-a-heap-dump-m66_linux_macos_windows).

## Profiling using Instruments Time Profiler

For more sophisticated CPU sampling, use the Time Profiler tool from Instruments. Instruments is macOS's performance analysis toolkit that comes with Xcode. The following steps assume that you've installed Xcode 12.0 or later.

After installing Xcode, run `xcode-select -s XCODE_FOLDER` from the command line to set up the Xcode folder for the command line tools. `XCODE_FOLDER` is where the Xcode is installed, and should be something like `/Applications/Xcode.app/Contents/Developer`.

Time Profiler provides a GUI to record traces, but is likely to be janky. To generate a trace from the terminal,

1. Start Chrome.
2. Run `xcrun xctrace record --template 'Time Profiler' --all-processes` in terminal to start tracing.
3. Perform actions that you want to profile, e.g. open a new tab page.
4. Back to the terminal, press Ctrl+C to terminate the profiling. A `.trace` profile file will be generated at the current working path.

In Step 2, if you know which process you are looking at, you can change the `--all-processes` to `--attach PID`.

To visualize a trace,

1. In Time Profiler, load the profile file by File > Open. You will see the traces of all threads and other stats like the Thermal State. You may filter the threads of your interest.
2. For official Google Chrome builds, you need to follow [this doc](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/mac/debugging.md#chrome-builds) to download the dSYMs files for symbolization.
3. In Time Profiler, use File > Symbols to load symbols. Loading symbols only for the main executable (Google Chrome) should be sufficient for other executables (Google Chrome Helper and Renderer) as well.

## Working with Minidumps

[See this
page.](https://sites.google.com/a/chromium.org/dev/developers/crash-reports)

## Disabling ReportCrash

macOS helpfully tries to write a crash report every time a binary crashes –
which happens for example when a test in unit\_tests fails. Since Chromium's
debug binaries are huge, this takes forever. If this happens, "ReportCrash" will
be the top cpu consuming process in Activity Monitor. You should disable
ReportCrash while you work on Chromium. Run `man ReportCrash` to learn how to do
this on your version of macOS. On 10.15, the command is

    launchctl unload -w /System/Library/LaunchAgents/com.apple.ReportCrash.plist
    sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.ReportCrash.Root.plist

Yes, you need to run this for both the normal user and the admin user.

## Processing Apple Crash Reports

If you get a Google Chrome crash report caught by ReportCrash/macOS, it will not
have symbols (every frame will be ChromeMain). To get a symbolized stack trace,
use the internal [crsym](httsp://goto.google.com/crsym) tool by simply pasting
the contents of an entire Apple crash report.

## Testing Other Locales

To test Chrome in a different locale, change your system locale via the System
Preferences. (Keep the preferences window open so that you can change the
locale back without needing to navigate through menus in a language you may not
know.)

## Using DTrace

DTrace is a powerful, kernel-level profiling and dynamic tracing utility. In
order to use DTrace, you need to (at least partially) disable System Integrity
Protection with ([see above](#Chrome-Builds)):

    csrutil enable --without dtrace

Using DTrace is beyond the scope of this document, but the following resources
are useful:

* [jgm's awesome introductory article](http://www.mactech.com/articles/mactech/Vol.23/23.11/ExploringLeopardwithDTrace/index.html)
* [Big Nerd Ranch's four-part series](https://www.bignerdranch.com/blog/hooked-on-dtrace-part-1/)
* [Defining static probes on macOS](http://www.macresearch.org/tuning-cocoa-applications-using-dtrace-custom-static-probes-and-instruments)
* [Examples](http://www.brendangregg.com/dtrace.html#Examples)
* [Tips from Sun](http://blogs.sun.com/bmc/resource/dtrace_tips.pdf)

DTrace examples on macOS: `/usr/share/examples/DTTk`

To get truss on macOS, use dtruss. That requires root, so use `sudo dtruss -p`
and to attach to a running non-root program.

## Memory/Heap Inspection

Chrome has [built-in memory instrumentation](../memory-infra/README.md) that can
be used to identify allocations and potential leaks.

MacOS also provides several low-level command-line tools that can be used to
inspect what's going on with memory inside a process. Note that most of these
tools only work effectively with system malloc and not PartitionAlloc. Since
[PartitionAlloc Everywhere](https://docs.google.com/document/d/1R1H9z5IVUAnXJgDjnts3nTJVcRbufWWT9ByXLgecSUM/preview),
you should additionally disable PartitionAlloc with these GN args:

```
use_partition_alloc_as_malloc = false
enable_backup_ref_ptr_support = false
```

Note that PartitionAlloc will still be used in Blink, just not for `malloc` in
other places anymore. See [PartitionAlloc build config](../../base/allocator/partition_allocator/build_config.md)
for disabling PartitionAlloc completely via GN arg `use_partition_alloc`.

**`heap`** summarizes what's currently in the malloc heap(s) of a process. It
shows a number of useful things:

* How much of the heap is used or free
* The distribution of block sizes
* A listing of every C++, Objective-C and CoreFoundation class found in the
  heap, with the number of instances, total size and average size.

It identifies C++ objects by their vtables, so it can't identify vtable-less
classes, including a lot of the lower-level WebCore ones like StringImpl. To
work around, temporarily added the `virtual` keyword to `WTF::RefCounted`'s
destructor method, which forces every ref-counted object to include a vtable
pointer identifying its class.

**`malloc_history`** identifies the stack backtrace that allocated every malloc
block in the heap. It lists every unique backtrace together with its number of
blocks and their total size. It requires that the process use malloc stack
logging, which is enabled if the environment variable MallocStackLogging is set
when it launches. The `env` command is handy for this:

    $ env MallocStackLogging=1 Chromium.app/Contents/MacOS/Chromium

Then in another shell you run

    $ malloc_history <pid> -allBySize

Watch out: the output is *big*.

**`leaks`** finds malloc blocks that have no pointers to them and are probably
leaked. It doesn't require MallocStackLogging, but it's more useful if it's on
because it can then show the backtrace that allocated each leaked block.

**`vmmap`** shows all the virtual-memory regions in the process's address space.
This is less useful since it doesn't say anything about individual malloc blocks
(except huge ones) but it can be useful for looking at things like static data
size, mapped files, and how much memory is paged out. The "-resident" flag shows
how much of each allocation is currently paged into RAM. See the man page for
details.

Notes:

* These are not going to be very useful on stripped binaries, and they're less
  useful in release builds.
* All of these except vmmap take several *minutes* to run, apparently because
  of the number of symbols in Chrome. They spend most of their time pegging
  one CPU down inside system code that's reading symbol tables from the
  binary. Be patient.
* Instruments is an application bundled with Xcode that provides GUI interfaces
  for many of these tools, including `sample` and `leaks`. Most Chromies prefer
  the command line tools, but Instruments can be useful. If running Instruments
  on a local build, expect to wait a few minutes for it to load symbols before
  it starts recording useful data

## Resources

The [Mac OS X Debugging Magic
Technote](https://developer.apple.com/technotes/tn2004/tn2124.html) contains a
wealth of (mostly outdated) information about various debugging options built in
to macOS.
