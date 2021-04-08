# Debugging Chromium on macOS

[TOC]

## Resources

The [Mac OS X Debugging Magic
Technote](http://developer.apple.com/technotes/tn2004/tn2124.html) contains a
wealth of information about various debugging options built in to macOS.

IMPORTANT: By default, Xcode has the "Load Symbols Lazily" preference set. As a
result, any symbols not in the main static library (99% of our code) won't be
visible to set breakpoints. The result is that you set breakpoints in the editor
window and they're ignored entirely when you run. The fix, however, is very
simple! Uncheck the "Load Symbols Lazily" checkbox in the "Debugging" panel in
preferences. Now all your breakpoints will work, at the expense of a little
longer load time in gdb. Well worth it, if you ask me.

ALSO IMPORTANT: If you include `fast_build=1` in your `GYP_DEFINES`, there is an
excellent chance the symbols you'll need for debugging will be stripped! You may
save yourself a lot of heartache if you remove this, rerun `gyp_chromium` and
rebuild before proceeding.

## Disabling ReportCrash

macOS helpfully tries to write a crash report every time a binary crashes –
which happens for example when a test in unit\_tests fails. Since Chromium's
debug binaries are huge, this takes forever. If this happens, "ReportCrash" will
be the top cpu consuming process in Activity Monitor. You should disable
ReportCrash while you work on Chromium. Run `man ReportCrash` to learn how to do
this on your version of macOS. On 10.8, the command is

    launchctl unload -w /System/Library/LaunchAgents/com.apple.ReportCrash.plist
    sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.ReportCrash.Root.plist

Yes, you need to run this for both the normal user and the admin user.

## Processing Apple Crash Reports

If you get a Google Chrome crash report caught by ReportCrash/OS X, it will not
have symbols (every frame will be ChromeMain). To get a symbolized stack trace,
use the internal [crsym](http://goto.google.com/crsym) tool by simply pasting
the contents of an entire Apple crash report.

## Debugging the renderer process

Xcode's built in gdb wrapper doesn't allow you to debug more than one process at
once and doesn't deal very well with debugging Chrome's subprocesses directly.
There are two different ways around this:

### (a) Run Chrome in a single process

(NOTE: this option is not recommended any more -- Chrome's single-process mode
is neither supported nor tested.)

1.  Edit the Executable settings for the Chromium app (make it the current
    executable, then choose Project &gt; Edit Active Executable).
2.  Switch to the Arguments tab and press the "+" button under the arguments
    list
3.  Type "`--single-process`" in the list.

From now on Chromium will launch in single-process mode when invoked through
this Xcode project, and the debugger will work fine. This obviously changes the
apps behavior slightly, but for most purposes the differences aren't
significant. If they are, though, you'll need to…

### (b) or, Attach Xcode's debugger to a renderer process after launch

1\. Launch the main executable from the Terminal (**not** through Xcode) and
pass in the `--renderer-startup-dialog` flag on the command line.   On
macOS this causes the renderer to print a message with its PID and then call
pause() immediately up on startup.  This has the effect of pausing execution of
the renderer until the process receives a signal (such as attaching the
debugger).

e.g.

    $ ~/dev/chrome//src/xcodebuild/Debug/Chromium.app/Contents/MacOS/Chromium --renderer-startup-dialog

the output should look like:

    ...
    [33215:2055:244180145280185:WARNING:/Users/Shared/bla/chrome/src/chrome/renderer/renderer_main.cc(48)]
    Renderer (33215) paused waiting for debugger to attach @ pid
    ...

So *33215* is the PID of the renderer process in question.

2\. Open chrome.xcodeproj in Xcode and select Run -> Attach To Process ->
Process ID .. 

3\. Click OK and off you go...

## Debugging out-of-process tests:

Similar to debugging the renderer process, simply attaching gdb to a
out-of-process test like browser\_tests will not hit the test code. In order to
debug a browser test, you need to run the test binary with  "`--single_process`"
(note the underscore in `single_process`). Because you can only run one browser
test in the same process, you're probably going to need to add `--gtest_filter`
as well. So your command will look like this:

    /path/to/src/xcodebuild/Debug/browser_tests --single_process --gtest_filter=GoatTeleporterTest.DontTeleportSheep

## UI Debugging

For UI Debugging, F-Script Anywhere is very useful.
Read <https://sites.google.com/a/chromium.org/dev/developers/f-script-anywhere>
for more information.

## Building with Ninja, Debugging with Xcode

See [the
instructions](https://sites.google.com/a/chromium.org/dev/developers/how-tos/debugging-on-os-x/building-with-ninja-debugging-with-xcode)
that discuss that scenario.

## Temporarily disabling the Sandbox

Disabling the sandbox can sometimes be useful when debugging, this can be
achieved by passing the --no-sandbox flag on the command line.  This will, for
example, allow writing out debugging information to a file from the Renderer
Process.
e.g.

    $ ~/dev/chrome//src/xcodebuild/Debug/Chromium.app/Contents/MacOS/Chromium --no-sandbox

## Tips on Debugging the Renderer Sandbox

Launch chrome with the `--enable-sandbox-logging` flag. This will cause a message
to be printed to /var/log/system.log every time an operation is denied by the
Sandbox (you can use Console.app to watch logfiles).  This is really useful for
debugging and can often provide an explanation for very puzzling problems.

You can also get the Sandbox to send a *SIGSTOP* to a process when the sandbox
denies functionality.  This allows you to attach with a debugger and continue
the execution from where it left off:

    $ sandbox-exec -p '(version 1) (allow default) (deny file-write\* (regex "foo") (with send-signal SIGSTOP))' touch foo

## Breakpoints Not Getting Hit in gdb

If a breakpoint you set isn't causing the debugger to stop, try one of these
solutions:

-   Uncheck "Load symbols lazily" In the Xcode -> Preferences -> Debugging
    dialog.
-   Manually insert a call to `Debugger()` in the code, this will forcefully break
    into the Debugger.

## Debugging in Release Mode

See "Preserving symbols in Release builds" below.

### Preserving symbols in Release builds

Profiling tools like Shark and 'sample' expect to find symbol names in the
binary, but in Release builds most symbols are stripped out. You can preserve
symbols by temporarily changing the build process, by adding
`mac_strip_release=0` to your GYP\_DEFINES, rerunning gclient runhooks, and
rebuilding (changing this define only relinks the main binary, it doesn't
recompile everything).

*(The above "Debugging in Release Mode" trick with the .dSYM file might work for
Shark/sample too; I haven't tried it yet. —snej)*

## Using DTrace

jgm's awesome introductory article:
[http://www.mactech.com/articles/mactech/Vol.23/23.11/ExploringLeopardwithDTrace/index.html
](http://www.mactech.com/articles/mactech/Vol.23/23.11/ExploringLeopardwithDTrace/index.html)
Defining static probes on macOS:
[http://www.macresearch.org/tuning-cocoa-applications-using-dtrace-custom-static-probes-and-instruments
](http://www.macresearch.org/tuning-cocoa-applications-using-dtrace-custom-static-probes-and-instruments)

[http://www.brendangregg.com/dtrace.html\#Examples
](http://www.brendangregg.com/dtrace.html#Examples)[http://blogs.sun.com/bmc/resource/dtrace\_tips.pdf
](http://blogs.sun.com/bmc/resource/dtrace_tips.pdf)

DTrace examples on macOS: /usr/share/examples/DTTk

To get truss on macOS, use dtruss. That requires root, so I often sudo dtruss -p
and attach to a running nonroot program.

## Testing other locales

To test Chrome in a different locale, change your system locale via the System
Preferences.  (Keep the preferences window open so that you can change the
locale back without needing to navigate through menus in a language you may not
know.)

## Memory/Heap Inspection

There are several low-level command-line tools that can be used to inspect
what's going on with memory inside a process.

'**heap**' summarizes what's currently in the malloc heap(s) of a process. (It
only works with regular malloc, of course, but Mac Chrome still uses that.) It
shows a number of useful things:

-   How much of the heap is used or free
-   The distribution of block sizes
-   A listing of every C++, Objective-C and CoreFoundation class found in the
    heap, with the number of instances, total size and average size.

It identifies C++ objects by their vtables, so it can't identify vtable-less
classes, including a lot of the lower-level WebCore ones like StringImpl. To
work around this I temporarily added the 'virtual' keyword to
WebCore::RefCounted's destructor method, which forces every ref-counted object
to include a vtable pointer identifying its class.

'**malloc\_history**' identifies the stack backtrace that allocated every malloc
block in the heap. It lists every unique backtrace together with its number of
blocks and their total size. It requires that the process use malloc stack
logging, which is enabled if the environment variable MallocStackLogging is set
when it launches. The 'env' command is handy for this:

    $ env MallocStackLogging=1 Chromium.app/Contents/MacOS/Chromium

Then in another shell you run

    $ malloc_history <pid> -all_by_size

Watch out: the output is *big*. I ran malloc\_history on a fairly bloated heap
and got 60MB of text.

'**leaks**' finds malloc blocks that have no pointers to them and are probably
leaked. It doesn't require MallocStackLogging, but it's more useful if it's on
because it can then show the backtrace that allocated each leaked block. (So far
I've seen only trivial leakage in Chrome.)

'**vmmap**' shows all the virtual-memory regions in the process's address space.
This is less useful since it doesn't say anything about individual malloc blocks
(except huge ones) but it can be useful for looking at things like static data
size, mapped files, and how much memory is paged out. I recommend the
"-resident" flag, which shows how much of each allocation is currently paged
into RAM. See the man page for details.


Notes:

-   These are not going to be very useful on stripped binaries, and they're less
    useful in release builds.
-   All of these except vmmap take several *minutes* to run, apparently because
    of the number of symbols in Chrome. They spend most of their time pegging
    one CPU down inside system code that's reading symbol tables from the
    binary. Be patient.
-   There are GUI apps in /Developer that do a lot of the same things, such as
    Instruments, MallocDebug and Shark. I (snej) personally find the
    command-line tools easier to understand, but YMMV.

## **Working with minidumps**

[See this
page.](https://sites.google.com/a/chromium.org/dev/developers/crash-reports)

## CrMallocErrorBreak

If you are looking at a crash report that ends in `CrMallocErrorBreak`, then
either a `malloc` or `free` call has failed with the given stacktrace. Chromium
overrides the empty function `malloc_error_break` in macOS's Libc
with `CrMallocErrorBreak`. The system calls this function as a debugging aide
that we've made fatal because it catches useful memory errors. Specifically,
`CrMallocErrorBreak` will be called (resulting in a crash) under the following
circumstances:

-   Attempting to free a pointer that was not allocated.
-   Attempting to free the same pointer more than once.
-   Freeing a pointer of size 0.
-   Freeing an unaligned pointer.
-   An internal checksum of the object being freed does not match. This usually
    indicates heap corruption!
-   Invalid checksums on the small or tiny free list. The system maintains a
    list of small allocations that it reuses to speed up things like allocations
    in a loop. A checksum mismatch usually indicates a use-after-free,
    double-free, or heap corruption.
-   Extra-large allocation failures. Normally all failures to allocate go
    through `CrMallocErrorBreak` but are
    not fatal because that is the job of Chromium's OOM killer. Extra-large
    allocations go through a different path and are sometimes killed here
    instead.

If you get a crash report that that ends in `CrMallocErrorBreak`,
it is likely not an issue with this feature. It is instead surfacing a
(sometimes serious) bug in your code or other code that is stomping on your
code's memory. Using Chromium's memory tools (ASan, HeapCheck, and Valgrind) is
a good start, if you can reproduce the problem.

##  Enabling high-DPI (aka "HiDPI" or "Retina") modes on standard-DPI hardware.

Under macOS 10.7 and above it's possible to fake "HiDPI" modes on standard-DPI
hardware.  This can be useful in testing up-scaling codepaths or high-DPI
resources.

1. Configure the OS to offer HiDPI modes:
    - EITHER [follow Apple's instructions to enable high resolution
        modes](http://developer.apple.com/library/mac/#documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Testing/Testing.html)
    - OR run the command-line: `sudo defaults write /Library/Preferences/com.apple.windowserver DisplayResolutionEnabled -bool YES`
2. Open the System Preferences -> Displays panel, select Scaled mode and scroll to the bottom to see modes marked "(HiDPI)".

Looking for `gdb`? It's been replaced with `lldb`. Use that it instead.

## Taking CPU Samples

A quick and easy way to investigate slow or hung processes is to use the sample
facility, which will generate a CPU sample trace. This can be done either in the
Terminal with the sample(1) command or by using Activity Monitor:

1.  Open Activity Monitor
2.  Find the process you want to sample (for "Helper" processes, you may want to consult the Chrome Task Manager)
3.  Double-click on the row
4.  Click the **Sample** button in the process's information window

After a few seconds, the sample will be completed. For official Google Chrome
builds, the sample should be symbolized using
[crsym](https://goto.google.com/crsym/). If you do not have access to crsym,
save the *entire* contents as a file and attach it to a bug report for later
analysis.

See also [How to Obtain a Heap
Dump](../memory-infra/heap_profiler.md#how-to-obtain-a-heap-dump-m66_linux_macos_windows).
