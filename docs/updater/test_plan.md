# Chromium Updater Test Plan

This is the test plan for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).

It should be read in conjunction with the
[functional specification](functional_spec.md) to assess functional coverage.

## Bots & Test Dimensions
The updater is covered by CQ and CI bots; the
[CI console](https://ci.chromium.org/p/chromium/g/chromium.updater/console)
shows the bots on which updater tests are run.

Except where noted, the updater's behavior is consistent across platforms and
is tested on macOS and Windows.

The updater may run at system-wide or per-user scope. The `updater_tests`
binary contains all user-scope tests, and the `updater_tests_system` binary
contains all system-scope tests. Except where otherwise noted, tests appear in
both binaries.

On macOS, `updater_tests_system` requires passwordless `sudo` to operate. In
local tests, this can usually be achieved by running `sudo echo;` before
running the tests. Updater bots running `updater_tests_system` have specifically
had passwordless sudo enabled.

On Windows, both UAC-enabled and normal Chromium configurations are tested.

## Test Binaries
To secure updates, the updater contains pinned public keys and update URLs.
This presents a challenge for testing, since the corresponding private keys
cannot be exposed to test infrastructure. The build system produces both a real
updater binary for distribution, and a "test" binary that is similar but with
extra test hooks compiled in. All tests use the test binary.

## Old Binaries
The updater updates itself. Covering this ability with tests presents a unique
challenge, since multiple versions of the updater have to be available to the
test. To support such tests, old versions of the updater are available in CIPD
and brought into the tree using `//DEPS` at `//third_party/updater/*/cipd`.

## Unit Testing
Much of the functionality of the updater is covered by unit testing. Unit tests
are defined in files ending in `(module)_test.*` in the source tree.

## Integration Testing
Integration tests cover the end-to-end behavior of the updater. They typically
rely on installing the updater, manipulating it, asserting on a result, and then
uninstalling the updater.

Because the integration tests actually install the updater, they usually don't
work with (and therefore aren't compiled into) `is_component_build=true` builds,
since component builds are generally not portable outside of the build output
directory.

Care must be taken when running the integration tests on a developer system,
especially in the `is_chrome_branded=true` context, since the tests install and
uninstall the updater, and attempt to leave the system clean (i.e. without any
GoogleUpdater installation). This may prevent the developer's copy of Google
Chrome (or other software) from being kept up to date.

Integration tests for system-scoped updaters frequently have to perform actions
at high privilege. This is accomplished by using an out-of-process test helper
binary (`updater_integration_tests_helper`) that runs at high privilege
(elevated via `sudo` on macOS). The helper binary run a gtest main so that the
commands it handles can use gtest asserts.

### Installation
IntegrationTest.Install tests that the updater can be installed on a clean OS,
that it is immediately active after installation, and then can be cleanly
uninstalled.

IntegrationTest.Handoff tests that the updater can be installed on a clean OS,
that it can install an app via a "/handoff" command line, and then can be
cleanly uninstalled.

Overinstall cases are tested by IntegrationTest.OverinstallWorking and
IntegrationTest.OverinstallBroken, to ensure that the updater can be installed
on an unclean OS, and as a post-condition of installation, the system has a
working and active updater (although it may not be the version just installed).

IntegrationTest.QualifyUpdater tests that the updater will perform its self-
test and then activate itself after installation/update, if an older working
instance of the updater is already present.

IntegrationTest.MigrateLegacyUpdater tests that the updater will import data
from legacy updaters (such as Omaha 3 or Keystone) when activating.

IntegrationTest.RecoveryNoUpdater tests that the recovery component
implementation can install and activate the updater on a machine where no
updater exists.

IntegrationTest.OfflineInstall, IntegrationTest.SilentOfflineInstall, and
IntegrationTest.LegacySilentOfflineInstall test that the updater can handle
handoffs from offline installers. *Windows Only*

### Updates
IntegrationTest.SelfUpdate tests that this version of the updater can run a
fake update on itself.

IntegrationTest.SelfUpdateFromOldReal tests that an older version of the updater
can successfully update to this version of the updater, and that this version
qualifies itself and activates.

IntegrationTest.UpdateApp tests that the updater can run an update on a
registered app.

IntegrationTest.SameVersionUpdate tests that the updater can perform a same-
versioned over-install of an application (such as in an installer context).

IntegrationTest.InstallDataIndex tests that the updater can handle an install
data index and transmit it to the server as part of the install request.

IntegrationTest.ReportsActive tests that the updater transmits active telemetry
for apps.

IntegrationTest.RotateLog tests that the updater rotates its log file after the
log file grows to a sufficient size.

IntegrationTest.ForceInstallApp tests that the updater will install an app,
when provided group policies that force installation of that app. *Windows Only*

IntegrationTest.MultipleWakesOneNetRequest tests that even if the updater wakes
often, it will only check for updates once (within the timeline of the test).

IntegrationTest.MultipleUpdateAllsMultipleNetRequests tests that if the updater
receives multiple commands to immediately check for updates, it does so (in
contrast to multiple wake commands).

IntegrationTest.LegacyUpdate3Web tests that the updater can be exercised using
the legacy COM APIs. *Windows Only*

IntegrationTest.UpdateServiceStress tests the IPC mechanisms to build confidence
that the updater can be started frequently.

### Services
IntegrationTest.LegacyProcessLauncher tests that the updater's process launcher
feature correctly launches a process when commanded to. *Windows Only*

IntegrationTest.LegacyAppCommandWeb tests that the updater, when set up with
an appropriate legacy app command and then commanded to run it, will do so.
*Windows Only*

IntegrationTest.LegacyPolicyStatus tests that the updater reports its policies
using the legacy COM APIs. *Windows Only*

IntegrationTest.UnregisterUnownedApp tests that the updater does not try to
update applications owned by other users or scopes. *macOS Only*

### Uninstallation
IntegrationTest.SelfUninstallOutdatedUpdater tests that an old version of the
updater removes itself after a new version of the updater has become active.

IntegrationTest.UninstallCmdLine tests that running the updater's uninstall
command uninstalls the updater only if the updater has been present for a
minimum number of startups, in the case that no app has ever been installed.
*Windows Only*

IntegrationTest.UnregisterUninstalledApp tests that if an app has been
uninstalled, the updater removes it from further update checks.

IntegrationTest.UninstallIfMaxServerWakesBeforeRegistrationExceeded tests that
the updater uninstalls itself if it has been present on the machine for a
while but no other apps have been installed.

IntegrationTest.UninstallUpdaterWhenAllAppsUninstalled tests that the updater
uninstalls itself when it no longer has other apps to keep up to date.

### Associated Tools
Associated tools should be covered by unit tests.

## Manual Testing
No routine manual testing is planned.
