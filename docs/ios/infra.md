# Continuous build and test infrastructure for Chromium for iOS

See the [instructions] for how to check out and build Chromium for iOS.

The Chromium projects use buildbot for continuous integration. This doc starts
with an overview of the system, then gives detailed explanations about each
part.

[TOC]

## Overview

Commits are made using the [commit queue], which triggers a series of try jobs
to compile and test the proposed patch against Chromium tip of tree before
actually making the commit. If the try jobs succeed the patch is committed. A
newly committed change triggers the builders (or "bots") to compile and test
the change again.

## Bots

Bots are slaves attached to a buildbot master (or "waterfall"). A buildbot
master is a server which polls for commits to a repository and triggers workers
to compile and test new commits whenever they are detected. [chromium.mac] is
the main waterfall for Mac desktop and iOS. [tryserver.chromium.mac] serves
as the try server for Mac desktop and iOS.

The bots know how to check out a given revision of Chromium, compile, and test.

### Code location

#### Master configs

The masters are configured in [tools/build], a separate repository which
contains various infra-related scripts.

#### Pollers

[chromium.mac] uses a `GitilesPoller` which polls the Chromium repository for
new commits using the [gitiles] interface. When a new commit is detected, the
bots are triggered.

#### Recipes

The bots run [recipes], which are scripts that specify their sequence of steps
located in [tools/build]. An iOS-specific [recipe module] contains common
functionality that the various [iOS recipes] use.

#### Configs

Because the recipes live in another repository, changes to the recipes don't
go through the Chromium [commit queue] and aren't tested on the [try server].
In order to allow bot changes to be tested by the commit queue, the recipes
for iOS are generic instead of bot-specific, and rely on configuration files
which live in master-specific JSON config files located in [src/ios/build/bots].
These configs define the `gn_args` to use during compilation as well as the
tests to run.

#### Scripts

The [test runner] is the script which installs and runs the tests, interprets
the results, and collects any files emitted by the test ("test data"). It can
be found in [src/ios/build/bots/scripts], which means changes to the test runner
can be tested on the [try server].

### Compiling with reclient

Reclient is the distributed build system used by Chromium. It reduces
compilation time by avoiding recompilation of objects which have already been
compiled elsewhere.

### Testing with swarming

Tests run on [swarming], a distributed test system used by Chromium. After
compilation, configured tests will be zipped up along with their necessary
dependencies ("isolated") and sent to the [swarming server] for execution. The
server issues tasks to its attached workers for execution. The bots themselves
don't run any tests, they trigger tests to be run remotely on the swarming
server, then wait and display the results. This allows multiple tests to be
executed in parallel.

## Try bots

Try bots are bots which test proposed patches which are not yet committed.

Request [try job access] in order to trigger try jobs against your patch. The
relevant try bots for an iOS patch are `ios-device` and `ios-simulator`. These
bots can be found on the Mac-specific [try server]. A try job is said to succeed
when the build passes (i.e. when the bot successfully compiles and tests the
patch).

`ios-device` compiles for the iOS device architecture (arm64) and runs no tests.
A build is considered successful so long as compilation is successful.

`ios-simulator` compiles for the iOS simulator architecture (x86_64), and runs
tests in the iOS [simulator]. A build is considered successful when both
compilation and all configured tests succeed.

`ios-device` and `ios-simulator` both compile using the version of [clang]
defined by the `CLANG_REVISION` in the Chromium tree.

### Scheduling try jobs using buildbucket

Triggering a try job and collecting its results is accomplished using
[buildbucket]. The service allows for build requests to be put into buckets. A
request in this context is a set of properties indicating things such as where
to get the patch. The try bots are set up to poll a particular bucket for build
requests which they execute and post the results of.

### Compiling with the analyzer

In addition to reclient, the try bots use another time-saving mechanism called
the [analyzer] to determine the subset of compilation targets affected by the
patch that need to be compiled in order to run the affected tests. If a patch is
determined not to affect a certain test target, compilation and execution of the
test target will be skipped.

## Configuring the bots

See the [configs code location](#Configs) for where to find the config files for
the bots. The config files are JSON which describe how the bot should compile
and which tests it should run. The config files are located in the configs
directory. The configs directory contains a named directory for each master. For
example:
```shell
$ ls ios/build/bots
OWNERS  scripts  tests  chromium.fyi  chromium.mac
```
In this case, configs are defined for iOS bots on [chromium.fyi] and
[chromium.mac]. Inside each master-specific directory are JSON config files
named after each bot. For example:
```shell
$ ls ios/build/bots/chromium.mac
ios-device.json ios-simulator.json
```
The `ios-device` bot on [chromium.mac] will read its configuration from
`chromium.mac/ios-device.json` in the configs directory.

### Example

```json
{
  "comments": [
    "Sample config for a bot."
  ],
  "gn_args": [
    "is_debug=true",
    "target_cpu=\"x64\"",
    "target_environment=\"simulator\""
  ],
  "tests": [
    {
      "app": "ios_chrome_unittests",
      "device type": "iPhone 5s",
      "os": "11.0",
      "xcode build version": "9A235"
    }
  ]
}
```
The `comments` key is optional and defines a list of strings which can be used
to annotate the config. You may want to explain why the bot exists and what it's
doing, particularly if there are extensive and atypical `gn_args`.

The `gn_args` key is a required list of arguments to pass to [GN] to generate
the build files. Two GN args are required, `is_debug` and `target_cpu`. Use
`is_debug` to define whether to compile for Debug or Release, and `target_cpu`
to define whether to compile for x64 or arm64. `target_environment` is used to
define whether to compile for simulator or device (as it cannot be deduced from
the cpu architecture as there exists both x64 and arm64 macOS devices). The iOS
bots typically perform Debug builds for the same architecture as the machine
running the tests (usually x64), and Release builds for arm64.

The `tests` key is an optional list of dictionaries defining tests to run. There
are two types of test dictionary, `app` and `include`. An `app` dict defines a
specific compiled app to run, for example:
```json
"tests": [
  {
    "app": "ios_chrome_unittests",
    "device type": "iPhone 5s",
    "os": "11.0",
    "xcode build version": "9A235"
  }
]
```

This dict says to run `ios_chrome_unittests` on an `iPhone 5s` running iOS
`11.0` using Xcode build version `9A235`. A test dict may optionally define a
list of `test args`, which are arguments to pass directly to the test on the
command line, and it may define a boolean value `xctest` to indicate whether the
test is an [xctest] \(default if unspecified is `false`\). For example:

```json
"tests": [
  {
    "app": "ios_chrome_unittests",
    "device type": "iPhone 5s",
    "os": "11.0",
    "test args": [
      "--foo",
      "--bar"
    ],
    "xcode build version": "9A235"
  },
  {
    "app": "ios_chrome_integration_egtests",
    "device type": "iPhone 5s",
    "os": "11.0",
    "xcode build version": "9A235",
    "xctest": true
  }
]
```
This defines two tests to run, first `ios_chrome_unittests` will be run with
`--foo` and `--bar` passed directly to the test on the command line. Next,
`ios_chrome_integration_egtests` will be run as an xctest. `"xctest": true`
must be specified for all xctests, it is an error to try and launch an xctest as
a regular test.

An `include` dict defines a list of tests to import from the `tests`
subdirectory in the configs directory. For example:
```json
"tests": [
  {
    "include": "common_tests.json",
    "device type": "iPhone 5s",
    "os": "11.0",
    "xcode build version": "9A235"
  }
]
```
This dict says to import the list of tests from the `tests` subdirectory and run
each one on an `iPhone 5s` running iOS `11.0` using Xcode `9A235`. Here's what
`common_tests.json` might look like:
```json
"tests": [
  {
    "app": "ios_chrome_unittests"
  },
  {
    "app": "ios_net_unittests"
  },
  {
    "app": "ios_web_unittests"
  },
]
```
Includes may contain other keys besides `app` which can then be omitted in the
bot config. For example if `common_tests.json` specifies:
```json
"tests": [
  {
    "app": "ios_chrome_integration_egtests",
    "xctest": true,
    "xcode build version": "9A235"
  }
]
```

Then the bot config may omit the `xctest` or `xcode build version` keys, for
example:

```json
{
  "comments": [
    "Sample config for a bot."
  ],
  "gn_args": [
    "is_debug=true",
    "target_cpu=\"x64\"",
    "target_environment=\"simulator\""
  ],
  "tests": [
    {
      "include": "common_tests.json",
      "device type": "iPhone 5s",
      "os": "11.0"
    }
  ]
}
```
Includes are not recursive, so `common_tests.json` may not itself include any
`include` dicts.

Some keywords such as `xcode build version` can also be set globally per build:

```json
{
  "comments": [
    "Sample config for a bot."
  ],
  "gn_args": [
    "is_debug=true",
    "target_cpu=\"x64\"",
    "target_environment=\"simulator\""
  ],
  "xcode build version": "9A235",
  "tests": [
    {
      "app": "ios_chrome_integration_egtests",
      "device type": "iPhone 5s",
      "os": "11.0"
    }
  ]
}
```

### Uploading compiled artifacts from a bot

A bot may be configured to upload compiled artifacts. This is defined by the
`upload` key. For example:
```json
{
  "comments": [
    "Sample config for a bot which uploads artifacts."
  ],
  "gn_args": [
    "is_debug=true",
    "target_cpu=\"x64\"",
    "target_environment=\"simulator\""
  ],
  "upload": [
    {
      "artifact": "Chromium.breakpad",
      "bucket": "my-gcs-bucket",
    },
    {
      "artifact": "Chromium.app",
      "bucket": "my-gcs-bucket",
      "compress": true,
    },
    {
      "artifact": "Chromium.breakpad",
      "symupload": "https://clients2.google.com/cr/symbol",
    }
  ]
}
```
After compilation, the bot will upload three artifacts. First the
`Chromium.breakpad` symbols will be uploaded to
`gs://my-gcs-bucket/<buildername>/<buildnumber>/Chromium.breakpad`. Next
`Chromium.app` will be tarred, gzipped, and uploaded to
`gs://my-gcs-bucket/<buildername>/<buildnumber>/Chromium.tar.gz`. Finally
the `Chromium.breakpad` symbols will be uploaded to the [breakpad] crash
reporting server where they can be used to symbolicate stack traces.

If `artifact` is a directory, you must specify `"compress": true`.

[analyzer]: ../../tools/mb
[breakpad]: https://chromium.googlesource.com/breakpad/breakpad
[buildbucket]: https://cr-buildbucket.appspot.com
[chromium.fyi]: https://build.chromium.org/p/chromium.fyi/waterfall
[chromium.mac]: https://build.chromium.org/p/chromium.mac
[clang]: ../../tools/clang
[commit queue]: https://dev.chromium.org/developers/testing/commit-queue
[gitiles]: https://gerrit.googlesource.com/gitiles
[GN]: ../../tools/gn
[instructions]: ./build_instructions.md
[iOS recipes]: https://chromium.googlesource.com/chromium/tools/build/+/main/scripts/slave/recipes/ios
[iOS simulator]: ../../testing/iossim
[recipe module]: https://chromium.googlesource.com/chromium/tools/build/+/main/scripts/slave/recipe_modules/ios
[recipes]: https://chromium.googlesource.com/infra/infra/+/HEAD/doc/users/recipes.md
[simulator]: https://developer.apple.com/library/content/documentation/IDEs/Conceptual/iOS_Simulator_Guide/Introduction/Introduction.html
[src/ios/build/bots]: ../../ios/build/bots
[src/ios/build/bots/scripts]: ../../ios/build/bots/scripts
[swarming]: https://chromium.googlesource.com/infra/luci/luci-py/+/main/appengine/swarming/
[swarming server]: https://chromium-swarm.appspot.com
[test runner]: ../../ios/build/bots/scripts/test_runner.py
[tools/build]: https://chromium.googlesource.com/chromium/tools/build
[try job access]: https://www.chromium.org/getting-involved/become-a-committer#TOC-Try-job-access
[try server]: https://build.chromium.org/p/tryserver.chromium.mac/waterfall
[tryserver.chromium.mac]: https://build.chromium.org/p/tryserver.chromium.mac/waterfall
[xctest]: https://developer.apple.com/reference/xctest
