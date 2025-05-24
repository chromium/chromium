# Usage of tools/lldb/lldbinit.py

Usage of Chromium's [lldbinit.py](../tools/lldb/lldbinit.py) is recommended when
debugging with lldb. This is necessary for source-level debugging when
`strip_absolute_paths_from_debug_symbols` is enabled [this is the default].

If you have not installed LLDB yet, run `sudo apt-get install lldb` to get it.

To use, add the following to your `~/.lldbinit`:

```
# So that lldbinit.py takes precedence.
script sys.path[:0] = ['/<your-path>/chromium/src/tools/lldb']
script import lldbinit
```

Replace `<your-path>` above with the absolute path containing your Chromium
repo.

Make sure the build configuration includes `is_debug=true`; this will set
`symbol_level=2` by default, which is required to view the content of
frame-level local variables.

For visualizer support for common pointer, string, and vector types in Chromium,
add the following:

```
script import chromium_visualizers
```

## How to attach to a process with lldb and start debugging

- Follow the instructions above to create your `~/.lldbinit` file; don't forget
  to put the correct absolute path to Chromium source in there.
- In your Chromium checkout, run `lldb out/Default/chrome` (or
  `out/Debug/chrome`)
    - On Mac, most likely,
      `lldb out/Default/Chromium.app/Contents/MacOS/Chromium`
- Keep lldb running and start Chromium separately with `--no-sandbox` flag:
    - On Linux, `out/Default/chrome --no-sandbox`
    - On Mac, `out/Default/Chromium.app/Contents/MacOS/Chromium --no-sandbox`
    - Note: if you start the process from lldb using
      `process launch -- --no-sandbox`, you will attach to the main browser
      process and will not be able to debug tab processes.
- In Chromium, go to the three-dot menu -> _More Tools_ -> _Task Manager_
- Note the process ID for the tab or process you want to debug.
- In the lldb shell:
    - Execute `process attach -p PID`. PID is the process ID of the process you
      want to debug.
        - Note: it might take a while. Once lldb attaches to the process, you
          will see a message `Process PID stopped` and some stack traces.
        - If you see an error message such as
          `attach failed: Operation not permitted`, it is probably due to
          [ptrace Protection](https://wiki.ubuntu.com/SecurityTeam/Roadmap/KernelHardening#ptrace_Protection).
          You can disable this feature using
          `echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`.
    - Now you can set breakpoints, e.g.,
      `breakpoint set -f inspector_overlay_agent.cc -l 627`.
    - Execute `cont` to continue the execution of the process.
    - Perform the actions that will trigger the breakpoint. lldb will stop the
      execution for you to inspect.
    - You can pause execution at any time by pressing Ctrl-C.
    - Type `help` to learn more about different lldb commands.
    - More open-source documentation can be found
      [here](https://developer.apple.com/library/archive/documentation/IDEs/Conceptual/gdb_to_lldb_transition_guide/document/lldb-basics.html#//apple_ref/doc/uid/TP40012917-CH2-SW1).
