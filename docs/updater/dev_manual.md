# Chromium Updater Developer's Manual

This manual provides information on how to develop the
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/),
including tips and tricks.

[TOC]

## Code Organization

## Bots & Lab
>**_NOTE:_** Knowledge in this section may become out-of-date as LUCI evolves
quickly.
### Builders / Testers
There are two sets of configuration files for our builders/testers. One is
for chromium-branded and locates in `src`. The other one is for chrome-branded
and locates in `src-internal`.

#### Chromium-branded (`src`)
  - Console: https://ci.chromium.org/p/chromium/g/chromium.updater/console
  - `tools/mb/mb_config.pyl`: specifies GN args.
  - `testing/buildbot/gn_isolate_map.pyl`: maps a GN label to GN targets, and
    provides test arguments, for example test timeout values.
  - `testing/buildbot/test_suites.pyl`: maps test suite name to GN label, and
    provides optional swarming dimensions.
  - `testing/buildbot/waterfalls.pyl`: maps tester to test suites names, and
    specifies OS, architecture etc.
  - `infra/config/subprojects/chromium/ci/chromium.updater.star`: defines our
    testers and builders and how they appear on the console.

Command to update json files after configure update:
  - `tools\mb\mb train` (if `mb_config.pyl` is changed).
  - `lucicfg generate .\infra\config\main.star` (if `chromium.updater.star`
    is changed).
  - `vpython3 .\testing\buildbot\generate_buildbot_json.py`

Reference CLs:
  - Add a tester: https://crrev.com/c/4068601
  - Update GN args: https://crrev.com/c/3656357

#### Chrome-branded (`src-internal`)
  - Console: https://ci.chromium.org/p/chrome/g/chrome.updater/console
  - `tools/mb/mb_config.pyl`: specifies GN args.
  - `testing/buildbot/gn_isolate_map.pyl`: maps a GN label to GN targets, and
    provides test arguments, for example test timeout values.
  - `testing/buildbot/test_suites.pyl`: maps test suite name to GN label, and
    provides optional swarming dimensions.
  - `testing/buildbot/waterfalls.pyl`: maps tester to test suites names, and
    specifies OS, architecture etc.
  - `infra/config/subprojects/chrome/ci/chrome.updater.star`: defines our
    testers and builders and how they appear on the console.

Command to update json files after configure update:
  - `..\src\tools\mb\mb train -f tools\mb\mb_config.pyl` (if `mb_config.pyl`
    is changed).
  - `lucicfg generate .\infra\config\main.star` (if `chrome.updater.star`
    is changed).
  - `vpython3 .\testing\buildbot\generate_testing_json.py`

Please note changes in `src-internal` needs to roll into chromium/src to take
effect. This could take hours until a CL authored by
`chromium-internal-autoroll@` lands. During transition, the configure files
could be in inconsistent state and leads to infra error.

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

* If `mb` command failed with error `isolate: original error: interactive login is required`, you need to login:

  ```
   tools/luci-go/isolate login
  ```

* If your test introduces dependency on a new app on macOS, you need to let
 `mb` tool know so it can correctly figure out the dependency. Example:
  https://crrev.com/c/3470143.

* To run tests on `Arm64`, the mb tool needs to be invoked as follows:

  ```
  .\tools\mb\mb run -v --swarmed --no-default-dimensions --internal -d pool chrome.tests.arm64 out\Default updater_tests_system -- --gtest_filter=LegacyAppCommandWebImplTest.FailedToLaunchStatus
  ```

* When system tests crash, the stack is missing from the swarming log. This can be avoided if you suppress the test bot mode:

  ```
  .\tools\mb\mb run -v --swarmed  --no-bot-mode out\Default updater_tests_system
  ```

### Accessing Bots
 TODO(crbug.com/40841197): Document how to remote into bots for debugging.

### Updating the Checked-In Version of the Updater
An older version of the updater is checked in under `//third_party/updater/*/cipd`.
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

## Developing
### Cross-platform Code
Where possible, cross-platform code is preferred to other alternatives. This
means that the source code of the updater is organized in sub-directories,
first by functionality (or feature), and second by platform name. For example,
the source code contains `updater\net` instead of `updater\mac\net`.

### Mind the dependencies

To enforce layering, there are enforced rules about what can be included in
certain modules. The rules checked by `GN` and the build breaks on bots if the
dependencies constraints are not satisfied.

Use the following command to check the target dependencies:
`gn check out\Default chrome/updater:* --check-generated --check-system`

## Building

### Configuring the build

After creating your build configuration directory via
[`gn gen`](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#Setting-up-the-build)
(this step is equivalent across all platforms), you will need to use
[`gn args`](https://www.chromium.org/developers/gn-build-configuration/) to
configure the build appropriately.

#### Flags required for building successfully

As of 2023-05-24, the updater cannot be built in component mode. It is also not
specifically designed to be built without the updater being enabled. You must
specify these options to `gn` via `gn args`:

```
is_component_build=false
enable_updater=true
```

Depending on other configuration options, the default `symbol_level`, 2, might
produce object files too large for the linker to handle (in debug builds).
Partial symbols, via `symbol_level=1`, fix this. Omitting almost all symbols
via `symbol_level=0` reuslts in a smaller and faster build but makes debugging
nearly impossible (call stacks will not be symbolicated).

#### Faster builds

Reclient is a distributed compiler service that allows you to compile Chromium
fast. The necessary Reclient binaries are distributed via CIPD and
automatically installed when you run gclient sync. After you've
set up Reclient, specify it in `gn args` with `use_remoteexec=true`.

To get started on Reclient, and for more information on how to use it, see
[macOS build instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md),
[Windows build instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/windows_build_instructions.md),
or [Google-internal documentation](go/building-chrome), if you are a Google
employee.

#### More release-like builds

Chromium projects build in debug mode by default. Release builds (also called
"opt", or "optimized", builds) are faster to link and run more efficiently;
they are, of course, much harder to debug. For a release build, add the
following to the build configuration's `gn args`:

```
is_debug=false
```

With a Google `src-internal` checkout, you can create a Chrome-branded build:

```
is_chrome_branded=true
include_branded_entitlements=false
```

Updater branding affects the path the updater installs itself to, among other
things. Differently-branded copies of Chromium Updater are intended to coexist
on a machine, operating independently from each other.

### Build output
The build generates the following main files:

  - `updater.exe` (Windows) / `ChromiumUpdater.app` (macOS): The actual updater
    application.
  - `UpdaterSetup.exe` (Windows): The self-extracting "metainstaller", suitable
    for tagging, signing, and further distribution.
  - `UpdaterSigning`: A collection of scripts and tools used to sign the
    updater.
  - `qualification_app` (.exe on Windows): A simple app run by each version of
    the updater prior to taking over as the active updater. Updaters will
    download and run whatever version is actually released (not what you've
    built), and the qualification app can be version skewed with the updater.
  - `ChromiumUpdaterUtil` (macOS): A utility program for debugging the updater.
  - `chrome/updater/.install` (macOS): A Keystone-compatible install script that
    drives the updater's installation during update.
  - `updater.zip`: A zip file containing all of the above, for uploading to the
    archive / signing infrastructure.

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
python3 tools/win/update_idl.py
```

### Build artifacts

Build outputs will land in the directory created by `gn gen` that you have been
providing to assorted `gn`, `ninja`, and `autoninja` commands. `updater.zip`
contains copies of the "final" outputs created by the build. `UpdaterSetup` is
probably what you want for installing the updater you have built.

## Code Coverage
Gerrit now down-votes the changes that do not have enough coverage. And it's
nice to have good coverage regardless. To improve code-coverage, we need to
know what are already covered and what are not.

#### Coverage on Gerrit
It's automatically generated. But the coverage shown is the combined result
from all OS platforms.

#### Coverage Dashboard
The [updater code coverage dashboard](https://analysis.chromium.org/coverage/p/chromium/dir?host=chromium.googlesource.com&project=chromium/src&ref=refs/heads/main&path=//chrome/updater/&platform=mac)
supports breakdown by OS platform or test type. But it is only for the code in
trunk.

#### Run Coverage Locally
We can quickly get OS-specific coverage result with the local changes:

* macOS/Linux
```
gn gen out/coverage --args="use_clang_coverage=true is_component_build=false is_chrome_branded=true is_debug=true use_debug_fission=true use_remoteexec=true symbol_level=2"

vpython3 tools/code_coverage/coverage.py  updater_tests -b out/coverage -o out/report -c 'out/coverage/updater_tests' -f chrome/updater
```

* Windows
```
gn gen out\coverage --args="use_clang_coverage=true is_component_build=false is_chrome_branded=true is_debug=true use_debug_fission=true use_remoteexec=true symbol_level=2"

vpython3 tools\code_coverage\coverage.py updater_tests -b out\coverage -o out\report -c out\coverage\updater_tests.exe  -f chrome/updater
```
The last command outputs an HTML file and you can open it in browser to see the
coverages.


## Unit tests and integration tests
### Running tests locally

The updater tests are available as `updater_tests` build target in the `out`
directory of the build.

In general, running branded unit tests locally is likely to break the updater
for the browser. To avoid this outcome when unsuspecting developers build and
run the branded updater tests, the test don't run locally unless the
developer sets an environment variable `ISOLATED_OUTDIR`. This is an
environment variable which is present on all bots (see the updater logs
section below). The presence of `ISOLATED_OUTDIR` does not preserve the
updater though. It only prevents the tests from being run.


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

Both the updater and the unit tests can create program logs.

#### Updater logs

The updater itself logs in the product directory.

#### Unit test logs

The unit tests log into a directory defined by the environment variable
`${ISOLATED_OUTDIR}`. When run by Swarming, the updater logs are copied
into `${ISOLATED_OUTDIR}` too, so that after the swarming task has completed,
both types of logs are available as CAS outputs. The logs for
`updater_tests_system` and `integration_test_helper` are merged into
`updater_tests_system.log`.

Non-bot systems can set up this environment variable to collect logs for
debugging when the tests are run locally.

## Testing src changes with trybots

In some cases, you will want to test the changes you make within
[chromium/src](https://chromium.googlesource.com/chromium/src.git) on
specific builders/testers before landing these changes. It is possible
to do this with the use of the trybots available on the
[tryserver.chromium.updater](https://ci.chromium.org/ui/p/chromium/g/tryserver.chromium.updater/builders)
 waterfall. The steps are as follows:

1. Find a trybot to run your tests with. All of the trybots on the
tryserver.chromium.updater waterfall have a corresponding builder and
tester, so find one that runs a workflow to test your changes.
2. Apply your configuration changes to chromium/src and upload a CL
using the typical workflow: `git cl upload`
3. Run `git cl try -B luci.chromium.try -b {TRYBOT_NAME}` with the
name of the trybot you found.
4. Monitor and debug any failures as you normally would for any
builder or tester.

### UI Strings & Localization
The strings for the metainstaller live in the //chrome/app/chromium_strings.grd
and //chrome/app/google_chrome_strings.grd files. This allows the updater
strings to utilize the Chromium repo's translation process instead of generating
its own. Having it in existing grd files also eliminates the need to onboard
updater specific grd files.

During the build process, the updater strings are embedded directly into the
metainstaller binary via `generate_embedded_i18n`. `generate_embedded_i18n` also
allows an `extractor_datafile`, which can define specific strings to pick out
from the originating grd file. This way, the metainstaller only has the strings
specific to the updater and not any of the other strings within the grd file.
When the `generate_embedded_i18n` is complete, it generates an
`updater_installer_strings.h` header, which contains macro definitions of the
message ids and the offsets. The strings are mapped with their var name appended
with `_BASE`. Then the  `_BASE` appended macros are defined to be the first
localization id in the list, in which case it is `_AF`.

An example from the `updater_installer_strings.h`
```
#define IDS_BUNDLE_INSTALLED_SUCCESSFULLY_AF 1600
#define IDS_BUNDLE_INSTALLED_SUCCESSFULLY_AM 1601

...

#define IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE IDS_BUNDLE_INSTALLED_SUCCESSFULLY_AF

...

#define DO_STRING_MAPPING \
  HANDLE_STRING(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE, IDS_BUNDLE_INSTALLED_SUCCESSFULLY) \
```

Within the metainstaller, an l10_util.h/cc has three functions to get localized
strings.
```
GetLocalizedString(int base_message_id)
GetLocalizedStringF(int base_message_id, const std::wstring& replacement)
GetLocalizedStringF(int base_message_id, std::vector<std::wstring> replacements)
```

One function for getting the literal string and two functions to get formatted
strings. `GetLocalizedString()` uses the base id plus the offset based on the
language to look through the binary's string table to get the correct, localized
string. The formatted strings utilize GetLocalizedString() to get the string and
then uses `base::ReplaceStringPlaceholders()` to remove the `$i` placeholders
within the string. With regards to picking the correct language to utilize for
the localized string, `base::win::i18n::GetUserPreferredUILanguageList()` is
used to get the preferred UI languages from MUI. If there are multiple languages
in the list, the first language in the list is picked.

#### Steps to add a new UI String to the updater

* Add the string to chrome/app/chromium_strings.grd and
chrome/app/google_chrome_strings.grd. For example:
```
  <message name="IDS_NO_NETWORK_PRESENT_ERROR"
           desc="Error message displayed in the main dialog when the Updater is unable to connect to the network.">
    Unable to connect to the Internet. If you use a firewall, please allowlist <ph name="PRODUCT_EXE_NAME">$1<ex>ChromiumUpdater.exe</ex></ph>.
  </message>
```
* Add the identifier for the string, for example `IDS_NO_NETWORK_PRESENT_ERROR`,
in chrome/updater/win/ui/resources/create_metainstaller_string_rc.py.
* Use the string identifier in the code. For example:
```
  GetLocalizedStringF(IDS_NO_NETWORK_PRESENT_ERROR_BASE, L"updater.exe");
```
* Add tests for the new string in the UI if applicable.
* Capture a screenshot of the UI with the new string.
* Save the screenshot as chrome\app\chromium_strings_grd\IDS_NO_NETWORK_PRESENT_ERROR.png
and chrome\app\google_chrome_strings_grd\IDS_NO_NETWORK_PRESENT_ERROR.png.
* Run `python3 tools/translation/upload_screenshots.py`
* This will generate chrome\app\chromium_strings_grd\IDS_NO_NETWORK_PRESENT_ERROR.png.sha1
and chrome\app\google_chrome_strings_grd\IDS_NO_NETWORK_PRESENT_ERROR.png.sha1.
* Add these `.sha1` files to your CL. Do not add the actual `.png` images to
your CL.
* Once the images are successfully uploaded via `upload_screenshots.py`, delete
them from your local enlistment.

However, if `upload_screenshots.py` encounters the following error, then the
screenshots have to be manually uploaded to
https://storage.cloud.google.com/chromium-translation-screenshots/.

```
ServiceException: 401 Anonymous caller does not have storage.objects.list
access to the Google Cloud Storage bucket. Permission 'storage.objects.list'
denied on resource (or it may not exist).
```

To manually upload each screenshot:
* Get the `sha1` generated from the tool. So for example, for
chrome/app/chromium_strings_grd/IDS_UNKNOWN_APPLICATION.png, the `sha1` is
`085c88707d854787e0c1310d93b519e93d906592`.
* Rename the `.png` file to the `sha1` hash name. So for example, rename
`chrome/app/chromium_strings_grd/IDS_UNKNOWN_APPLICATION.png` to
`chrome/app/chromium_strings_grd/085c88707d854787e0c1310d93b519e93d906592`.
* upload
`chrome/app/chromium_strings_grd/085c88707d854787e0c1310d93b519e93d906592` to
https://storage.cloud.google.com/chromium-translation-screenshots/.
* Edit the `content-type` from `application/octet-stream` to `image/png`.

## Troubleshooting

### Build errors

* **Maybe it's not you.** If you pulled from `origin/main` since your last
  successful build, or have never successfully built on your current branch,
  and the build errors you're seeing aren't obviously related to any changes
  you've made,
  [check the tree status](https://chromium-status.appspot.com/status_viewer).
  Did you pull down a broken version? If so, and the revert is in, pull again
  and see if it works better. Or skip checking the tree status and just try
  this as your first debugging step for build breaks after a pull.
* **Dependencies are a fast-moving target.** Remember to run `gclient sync -D`
  after every pull from `origin/main` _and_ every branch change. If you aren't
  sure whether you ran it, just run it, it's fast if you don't need it.
* **Symbols too big?** If your build is failing during linking, check your
  `gn args` to verify that `symbol_level=1` (or `0`) is present. If it's not,
  you're running into a known issue where the default symbol level, `2`,
  outputs symbols too large for the linker to comprehend.
