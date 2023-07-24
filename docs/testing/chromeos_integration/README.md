# crosier: **C**h**r**ome**OS** **I**ntegration/**E**2E-test **R**evamp

## Overview
Crosier is a project for running gtest based full browser test on ChromeOS
devices (DUT) or cros VM. The idea is similar to browser test. The test framework
would use gtest to launch a full browser on DUT/VM, and the test body would
be in the same process as browser process. Utilily APIs will be provided so
in the test body, it can access Chrome classes and objects like normal browser
tests, and can also access ChromeOS system services/daemons via utilities.

## Guidelines for using the framework
Given getting a DUT for local development is not easy for most developers, and
cros VM setup is harder than linux-chromeos environment, we should limit the
usage of this framework to avoid DUT testing when possible. The preferred order
when adding a new test:
- unit tests, browser tests in linux-chromeos environment
- unit tests, chromeos integration tests on cros VM
- unit tests, chromeos integration tests on a ChromeOS device (DUT)

Contacts: <crosier-team@google.com>
