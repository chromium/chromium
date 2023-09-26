# Tips for debugging on Linux

This page is for Chromium-specific debugging tips; learning how to run gdb is
out of scope.

[TOC]

## Symbolized stack trace

The sandbox can interfere with the internal symbolizer. Use `--no-sandbox` (but
keep this temporary) or an external symbolizer (see
`tools/valgrind/asan/asan_symbolize.py`).

Generally, do not use `--no-sandbox` on waterfall bots, sandbox testing is
needed. Talk to security@chromium.org.

## GDB

*** promo
GDB-7.7 is required in order to debug Chrome on Linux.
***

Any prior version will fail to resolve symbols or segfault.

### Setup

#### Build setup

In your build set the GN build variable `symbol_level = 2` for interactive
debugging. (`symbol_level = 1` only provides backtrace information). And while
release-mode debugging is possible, things will be much easier in a debug build.
Set your build args with `gn args out/<your_dir>` (substituting your build
directory), and set:

```
is_debug = true
symbol_level = 2
```

#### GDB setup

The Chrome build requires some GDB configuration for it to be able to find
source files. See [gdbinit](../gdbinit.md) to configure GDB. There is a similar
process for [LLDB](../lldbinit.md).

### Basic browser process debugging

    gdb -tui -ex=r --args out/Debug/chrome --disable-seccomp-sandbox \
        http://google.com

### Allowing attaching to foreign processes

On distributions that use the
[Yama LSM](https://www.kernel.org/doc/Documentation/security/Yama.txt) (that
includes Ubuntu and Chrome OS), process A can attach to process B only if A is
an ancestor of B.

You will probably want to disable this feature by using

    echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope

If you don't you'll get an error message such as "Could not attach to process".

Note that you'll also probably want to use `--no-sandbox`, as explained below.

### Multiprocess Tricks

#### Getting renderer subprocesses into gdb

Since Chromium itself spawns the renderers, it can be tricky to grab a
particular with gdb. This command does the trick:

```
chrome --no-sandbox --renderer-cmd-prefix='xterm -title renderer -e gdb --args'
```

The `--no-sandbox` flag is needed because otherwise the seccomp sandbox will
kill the renderer process on startup, or the setuid sandbox will prevent xterm's
execution.  The "xterm" is necessary or gdb will run in the current terminal,
which can get particularly confusing since it's running in the background, and
if you're also running the main process in gdb, won't work at all (the two
instances will fight over the terminal). To auto-start the renderers in the
debugger, send the "run" command to the debugger:

    chrome --no-sandbox --renderer-cmd-prefix='xterm -title renderer -e gdb \
        -ex run --args'

If you're using Emacs and `M-x gdb`, you can do

    chrome "--renderer-cmd-prefix=gdb --args"

*** note
Note: using the `--renderer-cmd-prefix` option bypasses the zygote launcher, so
the renderers won't be sandboxed. It is generally not an issue, except when you
are trying to debug interactions with the sandbox. If that's what you are doing,
you will need to attach your debugger to a running renderer process (see below).
***

You may also want to pass `--disable-hang-monitor` to suppress the hang monitor,
which is rather annoying.

You can also use `--renderer-startup-dialog` and attach to the process in order
to debug the renderer code. Go to
https://www.chromium.org/blink/getting-started-with-blink-debugging for more
information on how this can be done.

For utilities you can use `--utility-startup-dialog` to have all utilities
prompt, or `--utility-startup-dialog=data_decoder.mojom.DataDecoderService`
to debug only a particular service type.

#### Choosing which renderers to debug

If you are starting multiple renderers then the above means that multiple gdb's
start and fight over the console. Instead, you can set the prefix to point to
this shell script:

```sh
#!/bin/sh

echo "**** Child $$ starting: y to debug"
read input
if [ "$input" = "y" ] ; then
  gdb --args $*
else
  $*
fi
```

#### Choosing renderer to debug by URL

In most cases you'll want to debug the renderer which is loading a particular
site. If you want a script which will automatically debug the renderer which has
visited a given target URL and continue all other renderers, you can use the
following:

```sh
./third_party/blink/tools/debug_renderer out/Default/content_shell https://example.domain/path
```

The script also supports specifying a different URL than the navigation URL.
This is useful when the renderer you want to debug is not the top frame but one
of the subframes on the page. For example, you could debug a particular subframe
on a page with:

```sh
./third_party/blink/tools/debug_renderer -d https://subframe.url/path out/Default/content_shell https://example.domain/path
```

However, if you need more fine-grained control over which renderers to debug
you can run chrome or content_shell directly with the
`--wait-for-debugger-on-navigation` flag which will pause each renderer at the
point of navigation (when the URL is known).

This will result in a series of lines such as the following in the output:
```
...:content_switches_internal.cc(119)] Renderer url="https://example.domain/path" (PID) paused waiting for debugger to attach. Send SIGUSR1 to unpause.
```

You can signal the renderers you aren't interested in to continue running with:
```sh
kill -s SIGUSR1 <pid>
```

And debug the renderer you are interested in debugging with:
```sh
gdb -p <pid>
```

#### Debugging run_web_tests.py renderers

The `debug_renderer` script can also be used to debug the renderer running
a web test. To do so, simply call `run_{web,wpt}_tests.py` from `debug_renderer`
with all of the standard arguments for `run_{web,wpt}_tests.py`. For example:

```sh
./third_party/blink/tools/debug_renderer ./third_party/blink/tools/run_web_tests.py [run_web_test args]
```

#### Selective breakpoints

When debugging both the browser and renderer process, you might want to have
separate set of breakpoints to hit. You can use gdb's command files to
accomplish this by putting breakpoints in separate files and instructing gdb to
load them.

```
gdb -x ~/debug/browser --args chrome --no-sandbox --disable-hang-monitor \
    --renderer-cmd-prefix='xterm -title renderer -e gdb -x ~/debug/renderer \
    --args '
```

Also, instead of running gdb, you can use the script above, which let's you
select which renderer process to debug. Note: you might need to use the full
path to the script and avoid `$HOME` or `~/.`

#### Connecting to a running renderer

Usually `ps aux | grep chrome` will not give very helpful output. Try
`pstree -p | grep chrome` to get something like

```
        |                      |-bash(21969)---chrome(672)-+-chrome(694)
        |                      |                           |-chrome(695)---chrome(696)-+-{chrome}(697)
        |                      |                           |                           \-{chrome}(709)
        |                      |                           |-{chrome}(675)
        |                      |                           |-{chrome}(678)
        |                      |                           |-{chrome}(679)
        |                      |                           |-{chrome}(680)
        |                      |                           |-{chrome}(681)
        |                      |                           |-{chrome}(682)
        |                      |                           |-{chrome}(684)
        |                      |                           |-{chrome}(685)
        |                      |                           |-{chrome}(705)
        |                      |                           \-{chrome}(717)
```

Most of those are threads. In this case the browser process would be 672 and the
(sole) renderer process is 696. You can use `gdb -p 696` to attach.
Alternatively, you might find out the process ID from Chrome's built-in Task
Manager (under the Tools menu). Right-click on the Task Manager, and enable
"Process ID" in the list of columns.

Note: by default, sandboxed processes can't be attached by a debugger. To be
able to do so, you will need to pass the `--allow-sandbox-debugging` option.

If the problem only occurs with the seccomp sandbox enabled (and the previous
tricks don't help), you could try enabling core-dumps (see the **Core files**
section).  That would allow you to get a backtrace and see some local variables,
though you won't be able to step through the running program.

Note: If you're interested in debugging LinuxSandboxIPC process, you can attach
to 694 in the above diagram. The LinuxSandboxIPC process has the same command
line flag as the browser process so that it's easy to identify it if you run
`pstree -pa`.

#### Getting GPU subprocesses into gdb

Use `--gpu-launcher` flag instead of `--renderer-cmd-prefix` in the instructions
for renderer above.

#### Getting `browser_tests` launched browsers into gdb

Use environment variable `BROWSER_WRAPPER` instead of `--renderer-cmd-prefix`
switch in the instructions above.

Example:

```shell
BROWSER_WRAPPER='xterm -title renderer -e gdb --eval-command=run \
    --eval-command=quit --args' out/Debug/browser_tests --gtest_filter=Print
```

#### Plugin Processes

Same strategies as renderers above, but the flag is called `--plugin-launcher`:

    chrome --plugin-launcher='xterm -e gdb --args'

*** note
Note: For now, this does not currently apply to PPAPI plugins because they
currently run in the renderer process.
***

#### Single-Process mode

Depending on whether it's relevant to the problem, it's often easier to just run
in "single process" mode where the renderer threads are in-process. Then you can
just run gdb on the main process.

    gdb --args chrome --single-process

Currently, the `--disable-gpu` flag is also required, as there are known crashes
that occur under TextureImageTransportSurface without it. The crash described in
https://crbug.com/361689 can also sometimes occur, but that crash can be
continued from without harm.

Note that for technical reasons plugins cannot be in-process, so
`--single-process` only puts the renderers in the browser process. The flag is
still useful for debugging plugins (since it's only two processes instead of
three) but you'll still need to use `--plugin-launcher` or another approach.

### Printing Chromium types

gdb 7 lets us use Python to write pretty-printers for Chromium types. See
[gdbinit](../gdbinit.md)
to enable pretty-printing of Chromium types.  This will import Blink
pretty-printers as well.

Pretty printers for std types shouldn't be necessary in gdb 7, but they're
provided here in case you're using an older gdb. Put the following into
`~/.gdbinit`:

```
# Print a C++ string.
define ps
  print $arg0.c_str()
end

# Print a C++ wstring or wchar_t*.
define pws
  printf "\""
  set $c = (wchar_t*)$arg0
  while ( *$c )
    if ( *$c > 0x7f )
      printf "[%x]", *$c
    else
      printf "%c", *$c
    end
    set $c++
  end
  printf "\"\n"
end
```

[More STL GDB macros](http://www.yolinux.com/TUTORIALS/src/dbinit_stl_views-1.01.txt)

### JsDbg -- visualize data structures in the browser

JsDbg is a debugger plugin to display various Chrome data structures in a
browser window, such as the accessibility tree, layout object tree, DOM tree,
and others.
[Installation instructions are here](https://github.com/MicrosoftEdge/JsDbg),
and see [here](https://github.com/MicrosoftEdge/JsDbg/blob/master/docs/FEATURES.md)
for screenshots and an introduction.

For Googlers, please see [go/jsdbg](https://goto.google.com/jsdbg) for
installation instructions.

### Time travel debugging with rr

You can use [rr](https://rr-project.org) for time travel debugging, so you
can also step or execute backwards. This works by first recording a trace
and then debugging based on that.

You need an up-to-date version of rr, since rr is frequently updated to support
new parts of the Linux system call API surface that Chromium uses. If you have
any issues with the latest release version, try compiling rr
[from source](https://github.com/rr-debugger/rr/wiki/Building-And-Installing).

Once installed, you can use it like this:
```
rr record out/Debug/content_shell --single-process
rr replay
(rr) c
(rr) break blink::NGBlockNode::Layout
(rr) rc # reverse-continue to the last Layout call
(rr) jsdbg # run JsDbg as described above to find the interesting object
(rr) watch -l box_->frame_rect_.size_.width_.value_
(rr) rc # reverse-continue to the last time the width was changed
(rr) rn # reverse-next to the previous line
(rr) reverse-fin # run to where this function was called from
```

You can debug multi-process chrome using `rr -f [PID]`
for processes `fork()`ed from a [zygote process](zygote.md) without exec,
which includes renderer processes,
or `rr -p [PID]` for other processes.
To find the process id you can either run `rr ps` after recording, or for
renderer processes use `--vmodule=render_frame_impl=1` which will log a
message on navigations. Example:

```
$ rr record out/Debug/content_shell --disable-hang-monitor --vmodule=render_frame_impl=1 https://www.google.com/
rr: Saving execution to trace directory `...'.
...
[128515:128515:0320/164124.768687:VERBOSE1:render_frame_impl.cc(4244)] Committed provisional load: https://www.google.com/
```

From the log message we can see that the site was loaded into process 128515
and can set a breakpoint for when that process is forked.

```
rr replay -f 128515
```

If you want to call debugging functions from gdb that use `LOG()`,
then those functions need to disable the printing of timestamps using
[`SetLogItems`](https://source.chromium.org/search?q=SetLogItems&sq=&ss=chromium%2Fchromium%2Fsrc).
See `LayoutObject::ShowLayoutObject()` for an example of this, and
[issue 2829](https://github.com/rr-debugger/rr/issues/2829) for why it is needed.

If rr doesn't work correctly, the rr developers are generally quite responsive
to [bug reports](https://github.com/rr-debugger/rr/issues),
especially ones that have enough information so that
they don't have to build Chromium.

See Also:

* [The Chromium Chronicle #13: Time-Travel Debugging with RR](https://developer.chrome.com/blog/chromium-chronicle-13/)
* [@davidbaron demo using rr](https://twitter.com/davidbaron/status/1473761042278887433)
* [@davidbaron demo using pernosco](https://twitter.com/davidbaron/status/1475836824409022469)
(Googlers: see [go/pernosco](https://goto.google.com/pernosco))

### Graphical Debugging Aid for Chromium Views

The following link describes a tool that can be used on Linux, Windows and Mac under GDB.

[graphical_debugging_aid_chromium_views](../graphical_debugging_aid_chromium_views.md)

### Faster startup

Use the `gdb-add-index` script (e.g.
`build/gdb-add-index out/Debug/browser_tests`)

Only makes sense if you run the binary multiple times or maybe if you use the
component build since most `.so` files won't require reindexing on a rebuild.

See
https://groups.google.com/a/chromium.org/forum/#!searchin/chromium-dev/gdb-add-index/chromium-dev/ELRuj1BDCL4/5Ki4LGx41CcJ
for more info.

You can improve GDB load time significantly at the cost of link time by not
splitting symbols from the object files. In GN, set `use_debug_fission=false` in
your "gn args".

## Core files

`ulimit -c unlimited` should cause all Chrome processes (run from that shell) to
dump cores, with the possible exception of some sandboxed processes.

Some sandboxed subprocesses might not dump cores unless you pass the
`--allow-sandbox-debugging` flag.

If the problem is a freeze rather than a crash, you may be able to trigger a
core-dump by sending SIGABRT to the relevant process:

    kill -6 [process id]

## Breakpad minidump files

See [minidump_to_core.md](minidump_to_core.md)

## Running Tests

Many of our tests bring up windows on screen. This can be annoying (they steal
your focus) and hard to debug (they receive extra events as you mouse over them).
Instead, use `Xvfb` or `Xephyr` to run a nested X session to debug them, as
outlined on [testing/web_tests_linux.md](../testing/web_tests_linux.md).

### Browser tests

By default the `browser_tests` forks a new browser for each test. To debug the
browser side of a single test, use a command like

```
gdb --args out/Debug/browser_tests --single-process-tests --gtest_filter=MyTestName
```

**note the use of `single-process-tests`** -- this makes the test harness and
browser process share the outermost process.

The switch `--gtest_break_on_failure` can also be useful to automatically stop
debugger upon `ASSERT` or `EXPECT` failures.

To debug a renderer process in this case, use the tips above about renderers.

### Web tests

See [testing/web_tests_linux.md](../testing/web_tests_linux.md) for some tips. In particular,
note that it's possible to debug a web test via `ssh`ing to a Linux box; you
don't need anything on screen if you use `Xvfb`.

### UI tests

UI tests are run in forked browsers. Unlike browser tests, you cannot do any
single process tricks here to debug the browser. See below about
`BROWSER_WRAPPER`.

To pass flags to the browser, use a command line like
`--extra-chrome-flags="--foo --bar"`.

### Timeouts

UI tests have a confusing array of timeouts in place. (Pawel is working on
reducing the number of timeouts.) To disable them while you debug, set the
timeout flags to a large value:

*   `--test-timeout=100000000`
*   `--ui-test-action-timeout=100000000`
*   `--ui-test-terminate-timeout=100000000`

### To replicate Window Manager setup on the bots

Chromium try bots and main waterfall's bots run tests under Xvfb&openbox
combination. Xvfb is an X11 server that redirects the graphical output to the
memory, and openbox is a simple window manager that is running on top of Xvfb.
The behavior of openbox is markedly different when it comes to focus management
and other window tasks, so test that runs fine locally may fail or be flaky on
try bots. To run the tests on a local machine as on a bot, follow these steps:

Make sure you have openbox:

    apt-get install openbox

Start Xvfb and openbox on a particular display:

    Xvfb :6.0 -screen 0 1280x1024x24 & DISPLAY=:6.0 openbox &

Run your tests with graphics output redirected to that display:

    DISPLAY=:6.0 out/Debug/browser_tests --gtest_filter="MyBrowserTest.MyActivateWindowTest"

You can look at a snapshot of the output by:

    xwd -display :6.0 -root | xwud

Alternatively, you can use testing/xvfb.py to set up your environment for you:

    testing/xvfb.py out/Debug/browser_tests \
        --gtest_filter="MyBrowserTest.MyActivateWindowTest"

### BROWSER_WRAPPER

You can also get the browser under a debugger by setting the `BROWSER_WRAPPER`
environment variable.  (You can use this for `browser_tests` too, but see above
for discussion of a simpler way.)

    BROWSER_WRAPPER='xterm -e gdb --args' out/Debug/browser_tests

### Replicating try bot Slowness

Try bots are pretty stressed, and can sometimes expose timing issues you can't
normally reproduce locally.

You can simulate this by shutting down all but one of the CPUs
(http://www.cyberciti.biz/faq/debian-rhel-centos-redhat-suse-hotplug-cpu/) and
running a CPU loading tool (e.g., http://www.devin.com/lookbusy/). Now run your
test. It will run slowly, but any flakiness found by the try bot should replicate
locally now - and often nearly 100% of the time.

## Logging

### Seeing all LOG(foo) messages

Default log level hides `LOG(INFO)`. Run with `--log-level=0` and
`--enable-logging=stderr` flags.

Newer versions of Chromium with VLOG may need --v=1 too. For more VLOG tips, see
[the chromium-dev thread](https://groups.google.com/a/chromium.org/group/chromium-dev/browse_thread/thread/dcd0cd7752b35de6?pli=1).

### Seeing IPC debug messages

Run with `CHROME_IPC_LOGGING=1` eg.

    CHROME_IPC_LOGGING=1 out/Debug/chrome

or within gdb:

    set environment CHROME_IPC_LOGGING 1

If some messages show as unknown, check if the list of IPC message headers in
[chrome/common/logging_chrome.cc](/chrome/common/logging_chrome.cc) is
up to date. In case this file reference goes out of date, try looking for usage
of macros like `IPC_MESSAGE_LOG_ENABLED` or `IPC_MESSAGE_MACROS_LOG_ENABLED`.

## Profiling

See
https://sites.google.com/a/chromium.org/dev/developers/profiling-chromium-and-webkit
and [Linux Profiling](profiling.md).

## i18n

We obey your system locale. Try something like:

    LANG=ja_JP.UTF-8 out/Debug/chrome

If this doesn't work, make sure that the `LANGUAGE`, `LC_ALL` and `LC_MESSAGE`
environment variables aren't set -- they have higher priority than LANG in the
order listed. Alternatively, just do this:

    LANGUAGE=fr out/Debug/chrome

Note that because we use GTK, some locale data comes from the system -- for
example, file save boxes and whether the current language is considered RTL.
Without all the language data available, Chrome will use a mixture of your
system language and the language you run Chrome in.

Here's how to install the Arabic (ar) and Hebrew (he) language packs:

    sudo apt-get install language-pack-ar language-pack-he \
        language-pack-gnome-ar language-pack-gnome-he

Note that the `--lang` flag does **not** work properly for this.

On non-Debian systems, you need the `gtk30.mo` files. (Please update these docs
with the appropriate instructions if you know what they are.)

## Breakpad

See the last section of [Linux Crash Dumping](crash_dumping.md).

## Drag and Drop

If you break in a debugger during a drag, Chrome will have grabbed your mouse
and keyboard so you won't be able to interact with the debugger!  To work around
this, run via `Xephyr`. Instructions for how to use `Xephyr` are on the
[Running web tests on Linux](../testing/web_tests_linux.md) page.

## Tracking Down Bugs

### Isolating Regressions

Old builds are archived here:
https://build.chromium.org/buildbot/snapshots/chromium-rel-linux/
(TODO: does not exist).

`tools/bisect-builds.py` in the tree automates bisecting through the archived
builds. Despite a computer science education, I am still amazed how quickly
binary search will find its target.

### Screen recording for bug reports

    sudo apt-get install gtk-recordmydesktop

## Version-specific issues

### Google Chrome

Google Chrome binaries don't include symbols. Googlers can read where to get
symbols from
[the Google-internal wiki](http://wiki/Main/ChromeOfficialBuildLinux#The_Build_Archive).

### Ubuntu Chromium

Since we don't build the Ubuntu packages (Ubuntu does) we can't get useful
backtraces from them. Direct users to https://wiki.ubuntu.com/Chromium/Debugging

### Fedora's Chromium

Like Ubuntu, but direct users to
https://fedoraproject.org/wiki/TomCallaway/Chromium_Debug

### Xlib

If you're trying to track down X errors like:

```
The program 'chrome' received an X Window System error.
This probably reflects a bug in the program.
The error was 'BadDrawable (invalid Pixmap or Window parameter)'.
```

Some strategies are:

*   pass `--sync` on the command line to make all X calls synchronous
*   run chrome via [xtrace](http://xtrace.alioth.debian.org/)
*   turn on IPC debugging (see above section)

### Window Managers

To test on various window managers, you can use a nested X server like `Xephyr`.
Instructions for how to use `Xephyr` are on the
[Running web tests on Linux](../testing/web_tests_linux.md) page.

If you need to test something with hardware accelerated compositing
(e.g., compiz), you can use `Xgl` (`sudo apt-get install xserver-xgl`). E.g.:

    Xgl :1 -ac -accel glx:pbuffer -accel xv:pbuffer -screen 1024x768

## Mozilla Tips

https://developer.mozilla.org/en/Debugging_Mozilla_on_Linux_FAQ

## Google Chrome Symbol Files

Symbols for Google Chrome's official builds are available from
`https://edgedl.me.gvt1.com/chrome/linux/symbols/google-chrome-debug-info-linux64-${VERSION}.zip`
where ${VERSION} is any version of Google Chrome that has recently been served
to Stable, Beta, or Unstable (Dev) channels on Linux.
