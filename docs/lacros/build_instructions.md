# Checking out and building Lacros

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/lacros-build](https://goto.google.com/lacros-build) instead.


# Overview

There are roughly three ways to develop Lacros:

## Building and running on a Linux workstation.

This typically is faster for
development and requires no specialized hardware. The downside is that some
functionality is stubbed out (typically hardware / peripherals), some
functionality is different (some parts of graphics stack), and this is not
what we ship to users.

The workflow is similar to developing for Chrome on Linux. The core difference
is that instead of building and running a single binary [e.g. "chrome"] we
must build and run two binaries. "ash-chrome" is a ChromeOS runtime (shelf,
window manager, launcher, etc.) with some functionality stubbed out.
"lacros-chrome" is a binary that runs within the context of "ash-chrome".

See [Lacros: Build Instructions (Linux)](build_linux_lacros.md)

## Building on a Linux workstation and running on a physical ChromeOS device

This has much higher startup cost but is necessary for some features:
hardware-specific, performance, some aspects of graphics, supervised services
on DUT.

The workflow is similar to developing Chrome for ChromeOS. The core difference
is that instead of deploying a single binary "ash-chrome" via deploy_chrome.py,
we must build and deploy two binaries: "ash-chrome" and "lacros-chrome" via
deploy_chrome.py.

See [Lacros: Build Instructions (DUT)](build_dut_lacros.md)

## Building on a Linux workstation and running on a ChromeOS VM running on the same Linux workstation

This is not well supported but we have instructions
from developers who have successfully managed to do this.

If you do end up using multiple workflows, please be careful not to mix the
output directories as the binaries produced from each workflow are not
interchangeable.

# Testing
See [Test instructions](test_instructions.md).
