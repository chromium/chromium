# Debugging

It is possible to debug Fuchsia binaries using `zxdb`. For the sake of this
example, we will be using `base_unittests` as the test suite we wish to execute:

1. (From Chromium) Install your package(s) and its symbols onto the device.

   ```bash
   $ out/fuchsia/bin/install_base_unittests
   ```

2. (From Fuchsia source tree) Run the debugger.

  ```bash
  $ fx debug
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
  $ fx shell run fuchsia-pkg://fuchsia.com/base_unittests#meta/base_unittests.cmx
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