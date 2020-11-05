# Addressing Flaky WPTs

This document provides tips and tricks for reproducing and debugging flakes in
[Web Platform Tests](web_platform_tests.md) (WPTs). As WPTs are a form of Web
Test, you may also wish to refer to the documentation on [Addressing Flaky Web
Tests](web_tests_addressing_flake.md).

## Debugging flaky WPTs

See also the documentation in [Addressing Flaky Web
Tests](web_tests_addressing_flake.md#Debugging-flaky-Web-Tests).

### Loading WPT tests directly in content\_shell

WPT tests have to be loaded from a server, `wptserve`, to run properly. In a
terminal, run:

```
./third_party/blink/tools/run_blink_wptserve.py
```

This will start the necessary server(s), and print the ports they are listening
on. Most tests can just be loaded over the main HTTP server (usually
`http://localhost:8001`), although some may require using the HTTPS server
instead.

To load a WPT test in content\_shell, run:

```
out/Default/content_shell http://localhost:8001/path/to/test.html
```

Here, the `path/to/test.html` is relative to the root of
`third_party/blink/web_tests/external/wpt`, e.g. `dom/historical.html`.

**Caveat**: As with all Web Tests, running `content_shell` like this is not
equivalent to what `run_web_tests.py` runs. See the same section in [Addressing
Flaky Web
Tests](web_tests_addressing_flake.md#Loading-the-test-directly-in-content_shell)
for more details and some suggestions. In addition to that list, some WPTs
(usually security-related) also expect a real domain and may behave differently
when loaded from localhost.
