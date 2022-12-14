# Chromium Updater Developer's Manual

This manual provides information on how to develop the
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/),
including tips and tricks.

[TOC]

## Code Organization

### Cross-platform Code
Where possible, cross-platform code is preferred to other alternatives. This
means that the source code of the updater is organized in sub-directories,
first by functionality (or feature), and second by platform name. For example,
the source code contains `updater\net` instead of `updater\mac\net`.

## Bots & Lab
>**_NOTE:_** Knowledge in this section may become out-of-date as LUCI evolves
quickly.
### Adding Builders
* Update files as needed:
  - `testing/buildbot/waterfalls.pyl`
  - `infra/config/subprojects/chromium/ci/chromium.updater.star`
* Re-generate `chromium.updater.json`:
```
vpython3 .\testing\buildbot\generate_buildbot_json.py
```

* (Optional) Re-format the builder definition file if necessary.
```
lucicfg fmt .\infra\config\subprojects\chromium\ci\chromium.updater.star
```

* Generate builder property and configuration files:

```
lucicfg infra\config\main.star
git add .
```

* Reference CL: https://crrev.com/c/3864352

### Update builder configuration
Each builder has a configuration that governs the GN args. The mapping is
defined in file `tools/mb/mb_config.pyl`. Steps to update the config:
  * Modify `tools/mb/mb_config.pyl`.
  * Run command `./mb train` to update the expectations.
  * Example CL: https://crrev.com/c/3656357.

### Update tester configuration.
The parameters for invoking the updater unit tests when running in Buildbot are
defined in `testing/buildbot/gn_isolate_map.pyl`. After making changes to the
file, run `vpython3 .\testing\buildbot\generate_buildbot_json.py` to generate
the bot configurations, make a CL, and send it out.

### Run tests on swarming
`mb` tool can upload your private build target (and all the dependencies,
 based on build rule) to swarming server and run the target on bots. The
 upload may take quite some time if the target changed a lot since the last
 upload and/or your network is slow.
* Simple scenario:
  ```
  .\tools\mb\mb.bat run -v --swarmed .\out\Default updater_tests -- --gtest_filter=*Integration*
  ```
* Sometimes the mb tool may fail to match the testing OS (when doing
 cross-compile) or you may want to run the task on certain kind of bots.
This can be done by specifying bots dimension with switch `-d`. Remember
`--no-default-dimensions` is necessary to avoid dimension value conflict.
Example:
  ```
  .\tools\mb\mb.bat run --swarmed --no-default-dimensions -d pool chromium.win.uac -d os Windows-10 .\out\Default updater_tests_system -- --gtest_filter=*Install*
  ```
* `mb` can schedule tests in the pools managed by different swarming servers.
  The default server is
  [chromium-swarm.appspot.com](https://chromium-swarm.appspot.com/botlist?k=pool).
  To schedule tests to pools managed by
  [chrome-swarming.appspot.com](https://chrome-swarming.appspot.com/botlist?k=pool),
  for example `chrome.tests`, add `--internal` flag in the command line:
  ```
    tools/mb/mb run -v --swarmed --internal --no-default-dimensions -d pool chrome.tests -d os Windows-10 out/WinDefault updater_tests
  ```
* If your test introduces dependency on a new app on macOS, you need to let
 `mb` tool know so it can correctly figure out the dependency. Example:
  https://crrev.com/c/3470143.

### Accessing Bots
 TODO(crbug.com/1327486): Document how to remote into bots for debugging.

### Updating the Checked-In Version of the Updater
An older version of the updater is checked in under `//third_party/updater`.
This version of the updater is used in some integration tests. The updater is
pulled from
[CIPD](https://chrome-infra-packages.appspot.com/p/chromium/third_party/updater)
based on the versions specified in `//DEPS`. A system called `3pp` periodically
updates the packages in CIPD, based on a combination of the Chromium build
output and what is actually released through Omaha servers. The configuration
for 3pp can be found in `//third_party/updater/*/3pp`.

To update these copies of the updaters:
1.  Land whatever CLs need to be committed on trunk.
2.  Wait for builds to be available in CIPD that have the needed changes.
    *   Instead of waiting, you can instead modify the `fetch.py` scripts for
        3pp. For Chrome builds, make sure the build has been released in Omaha
        then update the fetch script with the desired version number. For
        Chromium, make sure the build exists in GCS (the
        chromium-browser-snapshots bucket), then update the min version in the
        script. The min version usually is different per-platform, since
        Chromium does not archive a version at every CL. After making these
        changes, 3pp will import the new versions within a few hours.
3.  Update //DEPS to point to the new versions.

## Building

### Cleaning the build output
Running `ninja` with `t clean` cleans the build out directory. For example:
```
ninja -C out\Default chrome/updater:all -t clean
```

### How to generate the cross-compilation IDL COM headers and TLB files

6 different build flavors need to be built in sequence. If you see errors
similar to the following:
```
midl.exe output different from files in gen/chrome/updater/app/server/win, see C:\src\temp\tmppbfwi0ds
To rebaseline:
  copy /y C:\src\temp\tmppbfwi0ds\* c:\src\chromium\src\third_party\win_build_output\midl\chrome\updater\app\server\win\x64
ninja: build stopped: subcommand failed.
```

You can then run the following command to update IDL COM files for all flavors:
```
python3 chrome/updater/tools/update_idl.py
```

## Debugging
### Debug into Windows update service

* Install updater: ```UpdaterSetup.exe --install [--system]```
> **_TIP:_**  Debugger may have trouble to find the symbols at the service
 side even if you add your build output directory to the symbol paths. To
workaournd the issue, you can copy `updater.exe*` to the versioned
installation directory.
* Set a breakpoint at the client side after service instantiation but before
calling into the service API. Most likely this will be somewhere in file
`update_service_proxy.*`.
* Start the client, wait for it to hit the break point. At this time, the
service process should have started.
* Find the server process, which contains the command line switch `--server`
(user-level server) or `--system --windows-service` (system-level server).
Start another debugger and attach the server process. Then set a server-side
breakpoint at the place you want to debug.
* Continue the client process.

### Logging

Both the updater and the unit tests can create program logs. The log destination
is different: the updater logs in the product directory, while the unit tests
log into a directory defined by the environment variable `${ISOLATED_OUTDIR}`.
When run by Swarming, the updater logs are copied into `${ISOLATED_OUTDIR}` too,
so that after the swarming task has completed, both types of logs are
available as CAS outputs.

Non-bot systems can set up this environment variable to collect logs for
debugging when the tests are run locally.
