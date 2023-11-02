# Debugging

[TOC]

It is possible to debug Fuchsia binaries using `zxdb`. For the sake of these
examples, we will be using `base_unittests` as the test suite we wish to
execute. These instructions assume that your Chromium build has the following gn
arguments:

```
is_debug = true
is_component_build = true
target_os = "fuchsia"
symbol_level = 2
```

## Manual debugging via the command line

1. (From Chromium) Install your package(s) and its symbols onto the device.

   ```bash
   out/fuchsia/bin/install_base_unittests --fuchsia-out-dir=/path/to/fuchsia/out/directory
   ```

2. (From Fuchsia source tree) Run the debugger.

   ```bash
   fx debug -- --build-dir /path/to/chromium/src/out/directory
   ```

3. Set up the debugger to attach to the process.

   ```
   [zxdb] attach base_unittests.cmx
   ```

4. Configure breakpoint(s).

   ```
   [zxdb] break base::GetDefaultJob
   ```

5. (In another terminal, from Fuchsia source tree) Run the test package.

   ```bash
   fx shell run fuchsia-pkg://fuchsia.com/base_unittests#meta/base_unittests.cmx
   ```

6. At this point, you should hit a breakpoint in `zxdb`.

   ```
   [zxdb] f
   ▶ 0 base::GetDefaultJob() • default_job.cc:18
     1 base::$anon::LaunchChildTestProcessWithOptions(…) • test_launcher.cc:335
     2 base::$anon::DoLaunchChildTestProcess(…) • test_launcher.cc:528
     3 base::TestLauncher::LaunchChildGTestProcess(…) • test_launcher.cc:877
     ...
   ```

7. Enjoy debugging! Steps 2 through 6 will also work for things like services
   which aren't run directly from the command line, such as WebEngine.
   Run `help` inside ZXDB to see what debugger commands are available.

## VS Code integration

1. Install the [zxdb](https://marketplace.visualstudio.com/items?itemName=fuchsia-authors.zxdb)
   extension.

2. Modify the `zxdb.command` setting in your Chromium workspace to this value:

   ```bash
   (cd /path/to/fuchsia ; fx debug -- --enable-debug-adapter --build-dir /path/to/chromium/src/out/directory)
   ```

3. Edit your debug launch configurations in `.vscode/launch.json`:

   ```json
   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Attach to base_unittests",
               "type": "zxdb",
               "request": "attach",
               "process": "base_unittests.cmx"
           }
       ]
   }
   ```

   You can add more configurations as needed.

4. Start the debug configuration in VS Code. You should get a terminal with zxdb
   running.

5. Launch the test suite in a terminal.

   ```bash
   out/fuchsia/bin/run_base_unittests -d --fuchsia-out-dir=/path/to/fuchsia/out/directory
   ```

6. Breakpoints set in the IDE should work.
