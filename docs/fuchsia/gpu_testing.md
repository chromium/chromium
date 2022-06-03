# Running GPU integration tests on Fuchsia

[TOC]

General instruction on running and debugging GPU integration tests can be
found [here](../gpu/gpu_testing.md).

Fuchsia uses either [web_engine_shell](../../fuchsia_web/webengine/test/README.md)
or the Chrome browser to run GPU integration tests. For the sake of this
example, we will be using `web_engine_shell` as the target browser and
`gpu_process` as the test suite we wish to execute. Build the target
`telemetry_gpu_integration_test_fuchsia` and run the appropriate commands:

## Hermetic emulation

The test script brings up an emulator, runs the tests on it, and shuts the
emulator down when finished.

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir
```

## Run on an physical device

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d
```

## Run on a device paved with Fuchsia built from source

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d
--fuchsia-out-dir=/path/to/fuchsia/outdir
```

Note that `fx serve` must be running for communication with the device to
succeed.

## Run on a device the host is connected to remotely via ssh

Note the `--ssh-config` flag, which should point to the config file used to set
up the connection between the host and the remote device.

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d --host=localhost
--ssh-config=/path/to/ssh/config
```
