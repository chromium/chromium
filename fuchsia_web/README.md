# `fuchsia.web` - Fuchsia WebEngine and Runners

This directory contains code related to the
[`fuchsia.web`](https://fuchsia.dev/reference/fidl/fuchsia.web) FIDL API.
Specifically, it contains the implementation of Fuchsia WebEngine and code
related to it, including the Runners that use it. Code in this
directory must not be used outside it and its subdirectories.

General information about Chromium on Fuchsia is
[here](../docs/fuchsia/README.md).

[TOC]

## Code organization
Each of the following subdirectories contain code for a specific Fuchsia
service:
* `./common` contains code shared by both WebEngine and Runners.
* `./runners`contains implementations of Fuchsia `sys.runner`.
    * `./runners/cast` Enables the Fuchsia system to launch Cast applications.
    * `./runners/web` Enables the Fuchsia system to launch HTTP or HTTPS URLs.
* `./shell` contains WebEngineShell, a simple wrapper for launching URLs in
WebEngine from the command line.
* `./webengine` contains the WebEngine implementation. WebEngine is an
implementation of
[`fuchsia.web`](https://fuchsia.dev/reference/fidl/fuchsia.web) that enables
Fuchsia Components to render web content using Chrome's Content layer.
* `./webinstance_host` contains code for WebEngine clients to directly
instantiate a WebInstance Component (`web_instance.cm`) using the WebEngine
package.

### Test code

There are 3 major types of tests within this directory:
* Unit tests: Exercise a single class in isolation, allowing full control
  over the external environment of this class.
* Browser tests: Spawn a full browser process and its child processes. The test
  code is run inside the browser process, allowing for full access to the
  browser code - but not other processes.
* Integration tests: Exercise the published FIDL API of a Fuchsia Component. For
  instance, `//fuchsia_web/webengine:web_engine_integration_tests` make use
  of the `//fuchsia_web/webengine:web_engine` component. The test code runs
  in a separate process in a separate Fuchsia Component, allowing only access to
  the published API of the component under test.

Integration tests are more resource-intensive than browser tests, which are in
turn more expensive than unit tests. Therefore, when writing new tests, it is
preferred to write unit tests over browser tests over integration tests.

As a general rule, test-only code should live in the same directory as the code
under test with an explicit file name, either `fake_*`, `test_*`,
`*_unittest.cc`, `*_ browsertest.cc` or `*_integration_test.cc`.

Test code that is shared across Components should live in `a dedicated ``test`
directory. For example, the `//fuchsia_web/webengine/test` directory, which
contains code shared by all browser tests, and
`//fuchsia_web/common/test`, which contains code shared by tests for both
WebEngine and Runners.

## Building and deploying the WebRunner service

When you build `web_runner`, Chromium will automatically generate scripts for
you that will automatically provision a device with Fuchsia and then install
`web_runner` and its dependencies.

To build and run `web_runner`, follow these steps:

1. (Optional) Ensure that you have a device ready to boot into Fuchsia.

    If you wish to have `web_runner` manage the OS deployment process, then you
    should have the device booting into
    [Zedboot](https://fuchsia.googlesource.com/zircon/+/master/docs/targets/usb_setup.md).

2. Build `web_runner`.

    ```bash
    $ autoninja -C out/fuchsia web_runner
    ```

3. Install `web_runner`.

    * For devices running Zedboot:

        ```bash
        $ out/fuchsia/bin/install_web_runner -d
        ```

    * For devices already booted into Fuchsia:

        You will need to add command line flags specifying the device's IP
        address and the path to the `ssh_config` used by the device
        (located at `$FUCHSIA_OUT_DIR/ssh-keys/ssh_config`):

        ```bash
        $ out/fuchsia/bin/install_web_runner -d --ssh-config $PATH_TO_SSH_CONFIG
        ```

4. Press Alt-Esc key on your device to switch back to terminal mode or run
`fx shell` from the host.

5. Launch a webpage.

    ```bash
    $ tiles_ctl add https://www.chromium.org/
    ```

6. Press Alt-Esc to switch back to graphical view if needed. The browser
window should be displayed and ready to use.

7. You can deploy and run new versions of Chromium without needing to reboot.

    First kill any running processes:

    ```bash
    $ killall context_provider.cmx; killall web_runner.cmx
    ```

    Then repeat steps 1 through 6 from the installation instructions, excluding
    step #3 (running Tiles).


### Closing a webpage

1. Press the Windows key to return to the terminal.

2. Instruct tiles_ctl to remove the webpage's window tile. The tile's number is
    reported by step 6, or it can be found by running `tiles_ctl list` and
    noting the ID of the "url" entry.

    ```bash
    $ tiles_ctl remove TILE_NUMBER
    ```
