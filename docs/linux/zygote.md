A zygote process is one that listens for spawn requests from a main process
and forks itself in response. Generally they are used because forking a process
after some expensive setup has been performed can save time and share extra
memory pages.

More specifically, on Linux, it allows to:
 * Amortize the runtime and memory cost of the dynamic loader's relocations,
   which is respectively ~6 MB and 60 ms/GHz per process.
   See [Appendix A](#appendix-a-runtime-impact-of-relocations) and
   [Appendix B](#appendix-b-memory-impact-of-relocations).
 * Amortize the runtime and memory cost for initializing common
   libraries, such as ICU, NSS, the V8 snapshot and anything else in
   `ContentMainRunnerImpl::Initialize()`. With the above, this saves
   up to ~8 MB per process. See [Appendix C](#appendix-c-overall-memory-impact).

Security-wise, the Zygote is responsible for setting up and bookkeeping the
[namespace sandbox](sandboxing.md).

Furthermore it is the only reasonable way to keep a reference to a binary
and a set of shared libraries that can be exec'ed. In the model used on Windows
and Mac, renderers are exec'ed as needed from the chrome binary. However, if the
chrome binary, or any of its shared libraries are updated while Chrome is
running, we'll end up exec'ing the wrong version. A version _x_ browser might be
talking to a version _y_ renderer. Our IPC system does not support this (and
does not want to!).

So we would like to keep a reference to a binary and its shared libraries and
exec from these. However, unless we are going to write our own `ld.so`, there's
no way to do this.

Instead, we exec the prototypical renderer at the beginning of the browser
execution. When we need more renderers, we signal this prototypical process (the
zygote) to fork itself. The zygote is always the correct version and, by
exec'ing one, we make sure the renderers have a different address space
randomisation than the browser.

The zygote process is triggered by the `--type=zygote` command line flag, which
causes `ZygoteMain` (in `chrome/browser/zygote_main_linux.cc`) to be run. The
zygote is launched from `content/browser/zygote_host/zygote_host_impl_linux.cc`.

Signaling the zygote for a new renderer happens in
`chrome/browser/child_process_launcher.cc`.

You can use the `--zygote-cmd-prefix` flag to debug the zygote process. If you
use `--renderer-cmd-prefix` then the zygote will be bypassed and renderers will
be exec'ed afresh every time.

## Appendix A: Runtime impact of relocations
Measured on a Z620:

    $ LD_DEBUG=statistics /opt/google/chrome-beta/chrome --help
    runtime linker statistics:
      total startup time in dynamic loader: 73899158 clock cycles
        time needed for relocation: 56836478 clock cycles (76.9%)
           number of relocations: 4271
           number of relocations from cache: 11347
           number of relative relocations: 502740
        time needed to load objects: 15789844 clock cycles (21.3%)

56836478 clock cycles -> ~56 ms/GHz

## Appendix B: Memory impact of relocations

    $ readelf -WS /opt/google/chrome-beta/chrome
    [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
    ...
    [25] .data.rel.ro      PROGBITS        0000000006a8b590 6a8a590 5b5500 00  WA  0   0 16
    ...
    Note: 0x5b5500  -> 5.98 MB

Actual impact in terms of memory pages that get shared due to CoW:

    $ cat /proc/.../smaps
    7fbdd1c81000-7fbdd2233000 r--p 06a5d000 fc:00 665771     /opt/google/chrome-unstable/chrome
    ...
    Shared_Dirty:       5796 kB

## Appendix C: Overall memory impact
    $ cat /proc/$PID_OF_ZYGOTE/smaps | grep Shared_Dirty | awk '{TOTAL += $2} END {print TOTAL}'
    8092  # KB for dirty pages shared with other processes (mostly forked child processes).
