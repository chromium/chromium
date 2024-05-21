# Metadata for Crosier tests

## Create a yaml file with test case details

Create a yaml file in the same location of your test source file. Example of
such yaml file:

```yaml
---
name: "LockScreen"
harness: "crosier"
category: "integration"
owners:
  - email: "test-owner-email@google.com"
  - email: "chromeos-sw-engprod@google.com"
hw_agnostic: False
criteria: |
  Tests that dbus messages for lid close trigger screen lock. This only tests
  the "lock on lid close" pref state.
cases:
  - id: "CloseLidDbusIntegration"
    tags: ["crosier:crosierdemosuite", "crosier:cq"]
  - id: "CloseLidPref"
    tags: ["crosier:crosierdemosuite", "crosier:cq", "informational"]
...
```

Pay specifal attention to the list of `tags` for each test case. Using these
tags, tests are being allocated to ChromeOS CI/CQ scheduling suites.

Following tags are recognized and supported:

* `crosier:crosierdemosuite` - test case will run daily in a dedicated,
non-critical test suite for stability and regression monitoring.
* `crosier:cq` - test case will run in global CQ and post-submit snapshot
builds.
* `informational` - test case will be considered as non-critical, i.e. not
included in the critical CQ test suite.
* `group:hw_agnostic` - test will run on VMs rather than on devices.

## Include new yaml file in Crosier binary build

The link to all yaml files should be added to the `crosier_metadata` rules that
is included by the Crosier `chromeos_integration_tests` rule of the
[BUILD.gn](https://chromium.googlesource.com/chromium/src/+/main/chrome/test/BUILD.gn)
file:

```
copy("crosier_metadata") {
    sources = [
        ...
        "../browser/ash/login/lock/lock_screen_integration_test.yaml",
        ...
        ...
    ]
    outputs = [ "$root_out_dir/crosier_metadata/{{source_file_part}}" ]
}

```
