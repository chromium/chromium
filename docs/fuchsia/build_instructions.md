# Checking out and building on Fuchsia

***If you have followed the instructions below and are still having trouble,
see [Contact information](README.md#contact-information).***

[TOC]

## System requirements

Building and running on Linux is officially supported, mac build is not. If you
are willing to run emulator based tests locally, KVM is required. You may check
if `/dev/kvm` exists on the system, or follow the [Enable KVM](#enable-kvm)
section.

1. Follow up [chromium for linux](../linux/build_instructions.md) to setup
chromium environment.

2. Edit your `.gclient` to add `fuchsia` to the `target_os` list. The file
   should look similar to this:

   ```
   solutions = [
     {
       "url": "https://chromium.googlesource.com/chromium/src.git",
       "managed": False,
       "name": "src",
       "custom_deps": {},
       "custom_vars": {}
     }
   ]
   target_os = ['fuchsia']
   ```

2. Run `gclient sync`

3. Create a build directory:

   ```shell
   $ gn gen out/fuchsia --args="is_debug=false dcheck_always_on=true is_component_build=false target_os=\"fuchsia\""
   ```

   You can add many of the usual GN arguments like `use_remoteexec = true`. In
   particular, when working with devices, consider using `is_debug = false` and
   `is_component_build = false` since debug and component builds can drastically
   increase run time and used space.

4. Build the target as you would for any other platform, you may specify the
targets to build at the end of the command line:
```shell
$ autoninja -C out/fuchsia
```

5. Most of the gtests based tests can be executed via
`out/fuchsia/bin/run_***_unittests`, e.g.
```shell
$ out/fuchsia/bin/run_base_unittests
```
   It starts a fresh new emulator instance and executes the tests on it under
   the hood. Also see the [Running test suites](#running-test-suites) section
   for other types of tests.

## Enable KVM
Under Linux, if your host and target CPU architectures are the same (e.g. you're
building for Fuchsia/x64 on a Linux/x64 host) then you can benefit from QEMU's
support for the KVM hypervisor:

1.  Install the KVM module for your kernel, to get a /dev/kvm device.
2.  Ensure that your system has a "kvm" group, and it owns /dev/kvm. You can do
    that by installing the QEMU system common package: `$ sudo apt-get
    install qemu-system-common`
3.  Add users to the "kvm" group, and have them login again, to pick-up the new
    group.
```shell
$ sudo adduser <user> kvm
$ exit [log in again]
```

## Running test suites

There are four types of tests available to run on Fuchsia:

1.  [Gtests](gtests.md)
2.  [GPU integration tests](gpu_testing.md)
3.  [Blink tests](web_tests.md)
4.  [Webpage tests](webpage_tests.md)

Check the documentations to learn more about how to run these tests.

Documentation for the underlying testing scripts work can be found
[here](test_scripts.md).