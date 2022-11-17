# Test Lacros using real devices or cros VM

## How to run a test?

### Lacros Tast test on DUT/VM

#### Run Tast from Chrome OS chroot

Currently, the official way to write and run Tast tests from workstation is to
use the ChromeOS Chroot, which needs chromiumos repository setup. Here are
reference documents:

[Checking out the ChromiumOS repo and create a chroot](https://chromium.googlesource.com/chromiumos/docs/+/main/developer_guide.md)

[Run Tast tests: Quick Start](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/quickstart.md)

If you don’t have a chromiumos repo already, it’s worth noting that it will take
at least a few hours to get one set up for the first time.

Example command to run the tast test in Chroot:
```
(chroot)% tast run ${your-device-ip}:${port} lacros.Basic
```
By default, the test will use the RootFS Lacros that is packaged as part of the
ChromeOS image, to test a newly built Lacros, first deploy the Lacros to the DUT:
```
./third_party/chromite/bin/deploy_chrome --build-dir=out_device_lacros/Release \
--device="${your-device-ip}:${port}" --nostrip --lacros
```

And then invoke tast command with a var:
```
(chroot)% tast run -var=lacros.DeployedBinary=/usr/local/lacros-chrome
${your-device-ip}:${port} lacros.Basic
```

#### Using chromium bots

If you don’t need to write or modify Tast tests, and just want to run them to
check if your change works with the current Tast tests, the quickest way is to
trigger the Chromium bots: lacros-amd64-generic-rel. Compared to 1 above, it is
very easy for those who do not have a ChromiumOS checkout locally. However, it
only supports running pre-built tests, so there’s no flexibility for modifying
or debugging the tests themselves, so If you hit any unexpected issues, please
fallback to using the Chrome OS chroot workflow described above.

#### Chromium-style GN/Ninja approach

One can also reproduce the 2nd workflow locally with the following steps:

First of all, build the lacros_all_tast_tests test target
```
autoninja -C out_device_lacros/Release/ lacros_all_tast_tests
```

Confirm that the build successfully generated a test runner script
```
ls out_device_lacros/Release/bin/run_lacros_all_tast_tests
```

Run tast test on the DUT
```
./out_device_lacros/Release/bin/run_lacros_all_tast_tests --board=${board} \
--device="${your-device-ip}:${port}" --logs-dir ~/test/logs
```

For how to set up a DUT, please see [Build DUT Lacros](build_dut_lacros.md).
If you don’t have a physical device, you can use a VM with the following command:
```
./out_device_lacros/Release/bin/run_lacros_all_tast_tests –board=amd64-generic \
 –use-vm --logs-dir ~/test/logs
```

Run unit tests on DUT

It’s almost identical to the Chromium-style GN/Ninja approach for running Tast
tests. Take cc_unittests for example:

First of all, build the cc_unittests test target
```
autoninja -C out_device_lacros/Release/ cc_unittests
```

Confirm that the build successfully generated a test runner script
```
ls out_device_lacros/Release/bin/run_cc_unittests
```

### How to write a new test?

#### Tast test

[tast-writing](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/writing_tests.md)

(Internal only)[go/lacros-tast-porting](https://goto.google.com/lacros-tast-porting)

(Internal only)[go/tast-failures](https://goto.google.com/tast-failures),
[go/tast-debugging-guide](https://goto.google.com/tast-debugging-guide)
Investigate test failures or debug.
