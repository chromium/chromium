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

> ⚠️ Note: on virtualized systems, e.g. cloudtops, the PMU counters may not
be available or may be broken. Use `-e cpu-clock` as a workaround.
Googlers, see [b/313526654](https://b.corp.google.com/issues/313526654).

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

## Heap Profiling

#### Dumping a profile of a running process

To programmatically generate a heap profile before exit, you can use gdb to
attach at any point:

1.  Attach gdb to the process: `$ gdb -p 12345`
2.  Cause it to dump a profile: `(gdb) p HeapProfilerDump("foobar")`
3.  The filename will be printed on the console you started Chrome from; e.g.
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
