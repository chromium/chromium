# Chromium Updater Developer's Manual

This manual provides information on how to develop the
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/),
including tips and tricks.

[TOC]

## Bots & Lab
>**_NOTE:_** Knowledge in this section may become out-of-date as LUCI evolves
quickly.
### Adding Builders
* Update files as needed:
  * `testing/buildbot/chromium.updater.json`
  * `testing/buildbot/waterfalls.pyl`
  * `infra/config/subprojects/chromium/ci/chromium.updater.star`
* Run the following command to generate LUCI config files:
    ```
    lucicfg infra\config\main.star
    ```
* Reference CL: https://crrev.com/c/3472270
### Update builder configuration
Each builder has a configuration that governs the GN args. The mapping is
defined in file `tools/mb/mb_config.pyl`. Steps to update the config:
  * Modify `tools/mb/mb_config.pyl`.
  * Run command `./mb train` to update the expectations.
  * Example CL: https://crrev.com/c/3656357.

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
* If your test introduces dependency on a new app on macOS, you need to let
 `mb` tool know so it can correctly figure out the dependency. Example:
  https://crrev.com/c/3470143.

### Accessing Bots
 TODO(crbug.com/1327486): Document how to remote into bots for debugging.

## Building

### Cleaning the build output
Running `ninja` with `t clean` cleans the build out directory. For example:
```
ninja -C out\Default chrome/updater:all -t clean
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
