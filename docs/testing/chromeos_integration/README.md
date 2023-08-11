# Crosier: **C**h**r**ome**OS** **I**ntegration/**E**2E-test **R**evamp

## Overview
Crosier is a project for running GTest-based integration tests on a ChromeOS
hardware Device Under Test (DUT) or in a ChromeOS Virtual Machine (VM). The idea
is similar to interactive_ui_tests. A runner uses GTest to launch a full browser
on DUT/VM, and the test body runs in the same process as the browser process.
Utility APIs will be provided so the test body can access Chrome classes and
objects like normal browser tests. The test can also access ChromeOS system
services/daemons via utilities (D-Bus wrappers, shell commands, etc.).

## Guidelines for using Crosier
Getting a DUT for local development is not easy for most developers and cros VM
setup is harder than using the linux-chromeos "emulator" environment. We should
limit the usage of Crosier and avoid DUT testing when possible.
Running ChromeOS integration tests on DUT is only recommended for tests
exercising hardware components (e.g. graphics tests) or testing communication
with OS daemons (e.g. Bluetooth daemon).

The preferred order when adding a new test:
- unit_tests, ash_unittests, browser_tests in linux-chromeos environment
- unit_tests, chromeos_integration_tests on a ChromeOS VM
- unit_tests, chromeos_integration_tests on a ChromeOS device (DUT)

Contacts: <crosier-team@google.com>
