# Mac Installer API
This document describes how macOS software integrates with Chromium Updater for
software updates.

## Design
macOS software updates are delivered as archives (often DMG or ZIP) that contain
an `.install` executable at the root of the archive. (Typically the archives
also contain an app bundle, i.e. the new version of the app.)

The updater will execute `.install`, passing data as positional command-line
arguments and environment variables.

The archive may also contain `.preinstall` and `.postinstall`, which are
executed before and after `.install` respectively. If any fail (exit with a
non-zero status), the remaining executables are not run. Collectively, these
three executables are called the "install executables".

## .install Execution Environment

### Command-Line Arguments
For backward compatibility, install executables are passed the following
arguments, in this order:

1. The absolute path to the unpacked update archive (i.e. the parent directory
of the install executable.)

2. The absolute path to the installation of the app, based on its
existence-checker value.

3. The version of the app before this update.

New install executables should instead depend on the environment variables below
rather than the above positional arguments.

### Environment Variables
The install executables are executed in an environment with the following
environment variables defined:

-   `PATH`: '/bin:/usr/bin:/Path/To/ksadmin'.
-   `PREVIOUS_VERSION`: The version of the app before this update.
-   `KS_TICKET_XC_PATH`: The absolute path to the installation of the app, based
 on its existence-checker value.
-   `KS_TICKET_AP`: The ap value of the currently-installed version of the app.
 (Note: "ap" was called "tag" in Keystone.)
-   `UPDATE_IS_MACHINE`: An indicator of the updater's scope.
    -   0 if the updater is per-user.
    -   1 if the updater is cross-user.
-   `SERVER_ARGS`: The arguments sent from the server, if any. Refer to
 protocol\_3\_1.md's description of `manifest` response objects.
-   `UNPACK_DIR`: The absolute path to the unpacked update archive (i.e. the
 parent directory of the install executable.)

## Updating Product Metadata
If the install executables succeed, the updater will automatically record the
new version of the application without any special action from the installers.

If the installers must also change other elements of the installation, such as
the brand, path, ap, or similar, they may do so by executing ksadmin, for
example by running
`ksadmin --register --product-id com.google.MyProductId --tag MyNewAp`.
