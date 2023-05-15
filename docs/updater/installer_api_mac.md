# Mac Installer API
This document describes how macOS software integrates with Chromium Updater for
software updates.

## Design
macOS software updates are delivered as archives (often DMG or ZIP) that contain
an `.install` executable at the root of the archive. (Typically the archives
also contain an app bundle, i.e. the new version of the app.)

The updater will execute all of the following executables in the archive, if
they exist, in order:

1. `.preinstall`
2. `.keystone_preinstall`
3. `.install`
4. `.keystone_install`
5. `.postinstall`
6. `.keystone_postinstall`

If any fail (exit with a non-zero status), the remaining executables are not
run and the installation fails.

If none exist, the installation fails.

## Execution Environment

### Environment Variables
Installer executables are executed in an environment with the following
environment variables:

-   `KS_TICKET_AP`: The ap value of the currently-installed version of the app.
 (Note: "ap" was called "tag" in Keystone.)
-   `KS_TICKET_SERVER_URL`: The URL used for update-checking with the server,
 regardless of what was in the Keystone ticket.
-   `KS_TICKET_XC_PATH`: The absolute path to the installation of the app, based
 on its existence-checker value.
-   `PATH`: '/bin:/usr/bin:/Path/To/ksadmin'.
-   `PREVIOUS_VERSION`: The version of the app before this update.
-   `SERVER_ARGS`: The arguments sent from the server, if any. Refer to
 the [Omaha protocol](protocol_3_1.md)'s description of `manifest` response
 objects.
-   `UPDATE_IS_MACHINE`: An indicator of the updater's scope.
    -   0 if the updater is per-user.
    -   1 if the updater is cross-user.
-   `UNPACK_DIR`: The absolute path to the unpacked update archive (i.e. the
 parent directory of the install executable.)
-   `%COMPANY%_USAGE_STATS_ENABLED`: where %COMPANY% is the uppercase company
    short name from branding.gni.
    -   0 if the updater does not send usage stats.
    -   1 if the updater sends usage stats.

### Command-Line Arguments
Installer executables are passed the following arguments, in this order:

1. The absolute path to the unpacked update archive (i.e. the parent directory
of the install executable.)
2. The absolute path to the installation of the app, based on its
existence-checker value.
3. The version of the app before this update.

New install executables should instead depend on environment variables rather
than positional arguments.

## Updating Product Metadata
If the install executables succeed, the updater will automatically record the
new version of the application without any special action from the installers.

If the installers must also change other elements of the installation, such as
the brand, path, ap, or similar, they may do so by executing ksadmin, for
example by running
`ksadmin --register --product-id com.google.MyProductId --tag MyNewAp`.
