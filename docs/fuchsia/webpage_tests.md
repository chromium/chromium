# Run Webpage Tests on Fuchsia

[TOC]

These tests allow developers to update Chrome/WebEngineShell on their
Fuchsia devices and navigate to a given url.

To build Chrome or WebEngineShell follow
[build instructions](build_instructions.md).

#### Usage

The following command will start an emulator, update WebEngineShell, and
navigate to [URL]:

```bash
$ ./build/fuchsia/test/run_test.py webpage --browser=web-engine-shell [URL] -C [OUTPUT_DIR]
```

To use with a persistent Fuchsia device, add the flag `--target-id=[FUCHSIA_NODENAME]`
to specify the device to use or `-d` if there is only one device connected.
