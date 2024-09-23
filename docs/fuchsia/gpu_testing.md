# Running GPU integration tests on Fuchsia

[TOC]

General instruction on running and debugging GPU integration tests can be
found [here](../gpu/gpu_testing.md).

Fuchsia uses either [web_engine_shell](../../fuchsia_web/shell/README.md)
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

## Additional flags

You can specify the following flags to help replicate failure cases:

- `--test-filter=<regex of test cases>`: Use this to filter test cases
- `--total-shards=<num shards>`: Specify total number of shards to split tests
  over. You would use this to replicate sharding on a bot.
- `--shard-index=<shard index>`: Specify shard index for splitting up the given
  tests.

## Run on an physical device

If ffx has already been set up to use the target device by default,
or if there is only one discoverable device on the host:

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d
```

Otherwise, specify the id of the target device:

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir --target-id=[TARGET_ID]
```

### Updating OS flag
You can update the OS of a device by specifying the path to an image and how
to check the OS:

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d --os-check=check
--system-image-dir=path/to/dir/containing/image
```

## Run on a device that needs packages built from Fuchsia source

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir -d
--repo=/path/to/fuchsia/outdir --no-repo-init
```

Note that `fx serve` should not be running, since the script
handles launching the package server from the Fuchsia output directory.

## Run on a device the host is connected to remotely via ssh

```bash
$ content/test/gpu/run_gpu_integration_test_fuchsia.py gpu_process
--browser=web-engine-shell --out-dir=/path/to/outdir --target-id=[::1]:8022
```

Note the this requires a remote tunnel to have been set up first.
