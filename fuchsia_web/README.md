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
