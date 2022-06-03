# Deploying content_shell and running web_tests on Fuchsia.

General instruction on running and debugging web_tests can be found
[here](../testing/web_tests.md).

Currently, only
[a small subset of web tests](../../third_party/blink/web_tests/SmokeTests)
can be run on Fuchsia. Build the target `blink_tests` first before running any
of the commands below:

## Hermetic emulation

The test script brings up an emulator, runs the tests on it, and shuts the
emulator down when finished.
```bash
$ third_party/blink/tools/run_web_tests.py -t <output-dir>  --platform=fuchsia
```

## Run on an physical device.

Note the `--fuchsia-host-ip` flag, which is the ip address of the test host that
the Fuchsia device uses to establish a connection.

```bash
$ third_party/blink/tools/run_web_tests.py -t <output-dir> --platform=fuchsia
--device=device --fuchsia-target-cpu=<device-cpu-arch>
--fuchsia-out-dir=/path/to/fuchsia/outdir --fuchsia-host-ip=test.host.ip.address
```