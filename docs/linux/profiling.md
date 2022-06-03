# Linux Profiling

How to profile Chromium on Linux.

See
[Profiling Chromium and WebKit](https://sites.google.com/a/chromium.org/dev/developers/profiling-chromium-and-webkit)
for alternative discussion.

## CPU Profiling

gprof: reported not to work (taking an hour to load on our large binary).

oprofile: Dean uses it, says it's good. (As of 9/16/9 oprofile only supports
timers on the new Z600 boxes, which doesn't give good granularity for profiling
startup).

TODO(willchan): Talk more about oprofile, gprof, etc.

Also see
https://sites.google.com/a/chromium.org/dev/developers/profiling-chromium-and-webkit

### perf

`perf` is the successor to `oprofile`. It's maintained in the kernel tree, it's
available on Ubuntu in the package `linux-tools`.

To capture data, you use `perf record`. Some examples:

```shell
# captures the full execution of the program
perf record -f -g out/Release/chrome
# captures a particular pid, you can start at the right time, and stop with
# ctrl-C
perf record -f -g -p 1234
perf record -f -g -a  # captures the whole system
```

Some versions of the perf command can be confused by process renames. Affected
versions will be unable to resolve Chromium's symbols if it was started through
perf, as in the first example above. It should work correctly if you attach to
an existing Chromium process as shown in the second example. (This is known to
be broken as late as 3.2.5 and fixed as early as 3.11.rc3.g36f571. The actual
affected range is likely much smaller. You can download and build your own perf
from source.)

The last one is useful on limited systems with few cores and low memory
bandwidth, where the CPU cycles are shared between several processes (e.g.
chrome browser, renderer, plugin, X, pulseaudio, etc.)

To look at the data, you use:

    perf report

This will use the previously captured data (`perf.data`).

### google-perftools

google-perftools code is enabled when the `use_allocator` gn variable is set
to `tcmalloc` (currently the default). That will build the tcmalloc library,
including the cpu profiling and heap profiling code into Chromium. In order to
get stacktraces in release builds on 64 bit, you will need to build with some
extra flags enabled by setting `enable_profiling = true` in args.gn

In order to enable cpu profiling, run Chromium with the environment variable
`CPUPROFILE` set to a filename.  For example:

    CPUPROFILE=/tmp/cpuprofile out/Release/chrome

After the program exits successfully, the cpu profile will be available at the
filename specified in the CPUPROFILE environment variable. You can then analyze
it using the pprof script (distributed with google-perftools, installed by
default on Googler Linux workstations). For example:

    pprof --gv out/Release/chrome /tmp/cpuprofile

This will generate a visual representation of the cpu profile as a postscript
file and load it up using `gv`. For more powerful commands, please refer to the
pprof help output and the google-perftools documentation.

Note that due to the current design of google-perftools' profiling tools, it is
only possible to profile the browser process.  You can also profile and pass the
`--single-process` flag for a rough idea of what the render process looks like,
but keep in mind that you'll be seeing a mixed browser/renderer codepath that is
not used in production.

For further information, please refer to
http://google-perftools.googlecode.com/svn/trunk/doc/cpuprofile.html.

## Heap Profiling

### google-perftools

#### Turning on heap profiles

Follow the instructions for enabling profiling as described above in the
google-perftools section under CPU Profiling.

To turn on the heap profiler on a Chromium build with tcmalloc, use the
`HEAPPROFILE` environment variable to specify a filename for the heap profile.
For example:

    HEAPPROFILE=/tmp/heapprofile out/Release/chrome

After the program exits successfully, the heap profile will be available at the
filename specified in the `HEAPPROFILE` environment variable.

Some tests fork short-living processes which have a small memory footprint. To
catch those, use the `HEAP_PROFILE_ALLOCATION_INTERVAL` environment variable.

#### Dumping a profile of a running process

To programmatically generate a heap profile before exit, use code like:

    #include "third_party/tcmalloc/chromium/src/google/heap-profiler.h"

    // "foobar" will be included in the message printed to the console
    HeapProfilerDump("foobar");

For example, you might hook that up to some action in the UI.

Or you can use gdb to attach at any point:

1.  Attach gdb to the process: `$ gdb -p 12345`
1.  Cause it to dump a profile: `(gdb) p HeapProfilerDump("foobar")`
1.  The filename will be printed on the console you started Chrome from; e.g.
    "`Dumping heap profile to heap.0001.heap (foobar)`"

#### Analyzing dumps

You can then analyze dumps using the `pprof` script (distributed with
google-perftools, installed by default on Googler Linux workstations; on Ubuntu
it is called `google-pprof`). For example:

    pprof --gv out/Release/chrome /tmp/heapprofile

This will generate a visual representation of the heap profile as a postscript
file and load it up using `gv`. For more powerful commands, please refer to the
pprof help output and the google-perftools documentation.

(pprof is slow. Googlers can try the not-open-source cpprof; Evan wrote an open
source alternative [available on github](https://github.com/martine/hp).)

#### Sandbox

Sandboxed renderer subprocesses will fail to write out heap profiling dumps. To
work around this, turn off the sandbox (via `export CHROME_DEVEL_SANDBOX=`).

#### Troubleshooting

*   "Hooked allocator frame not found": build with `-Dcomponent=static_library`.
    `tcmalloc` gets confused when the allocator routines are in a different
    `.so` than the rest of the code.

#### More reading

For further information, please refer to
http://google-perftools.googlecode.com/svn/trunk/doc/heapprofile.html.

## Paint profiling

You can use Xephyr to profile how chrome repaints the screen. Xephyr is a
virtual X server like Xnest with debugging options which draws red rectangles to
where applications are drawing before drawing the actual information.

    export XEPHYR_PAUSE=10000
    Xephyr :1 -ac -screen 800x600 &
    DISPLAY=:1 out/Debug/chrome

When ready to start debugging issue the following command, which will tell
Xephyr to start drawing red rectangles:

    kill -USR1 `pidof Xephyr`

For further information, please refer to
http://cgit.freedesktop.org/xorg/xserver/tree/hw/kdrive/ephyr/README.
