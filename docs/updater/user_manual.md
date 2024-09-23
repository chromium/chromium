# Chromium Updater User Manual

This is the user manual for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).

[TOC]

## Overview

Chromium Updater is an out-of-process service that finds and applies updates to
software. Integrating with Chromium Updater requires:

1. Installing Chromium Updater, if it is not present.
2. Registering your application for updates with Chromium Updater.
3. Performing any necessary server-side configuration to serve updates.

Additionally, applications should take care to:

1. Invoke the updater command line when they uninstall, if possible.

## Application Identity

Chromium Updater identifies each application by means of an "App ID". An app ID
can be any ASCII string. For example, the App ID of Google Chrome (Windows) is
`{8A69D345-D564-463C-AFF1-A69D9E530F96}`, and the App ID of Google Chrome
(macOS) is `COM.GOOGLE.CHROME`.

Acquiring a unique App ID is the first step to integrating with the updater.
Consult your organization's update server team to obtain an ID.

App IDs are case-insensitive.

## Installing the Updater

Chromium Updater can be installed in one of two scopes: system wide, or for the
current OS user only. System installations are appropriate if the software's
installation requires administrative privileges or should be shared between
multiple OS users. System installations require the user to have admin
privileges at the time of the installation, but thereafter silently keep the
software up to date.

### Windows

On Windows, applications are most commonly distributed by means of
UpdaterSetup.exe, a "tagged metainstaller" that first installs the updater,
then registers the application with the updater and asks the updater to "update"
(install) it. This has the advantage that even if UpdaterSetup.exe is old, the
newest version of the application is always installed on the system.
UpdaterSetup.exe also provides UI and manages integration with the updater.

UpdaterSetup.exe can also be used to install the updater alone, by running
`UpdaterSetup.exe --install --system`. (For user installs, elide `--system`.)

For security reasons, applications that install at system scope must install
into `C:\Program Files` or a similar path that non-admins don't have write
access to. System-scope installers are run with system privileges, and writing
or deleting paths under user control as system creates a privilege escalation
vulnerability on the system.

When an application installer successfully runs, it should write
`(HKCU or HKLM)\SOFTWARE\{Company}\Update\Clients\{AppID}` → `pv` to the version
that was installed.

### macOS

On macOS, applications are most commonly distributed either by means of a PKG
installer or a "drag-install" experience in a mountable DMG.

Applications installing via a PKG installer should bundle and install
{Company}Updater.pkg as part of their installation. Such an installation is
always system-wide. The application should run a postinstall script registering
itself with the updater by calling
`/Library/Application Support/{Company}/{Company}Updater/Current/{Company}Updater.app/Contents/Helpers/{Company}SoftwareUpdater.bundle/Contents/Helpers/ksadmin -r -P product_id -v version -x path_to_application_bundle -S`.

Applications installing via a DMG experience must set up the updater during
first run. These applications should embed the updater in their app bundle as a
["Helper"](https://developer.apple.com/documentation/bundleresources/placing_content_in_a_bundle),
and install by running
`{App Bundle}/Contents/Helpers/{Company}Updater.app/Contents/MacOS/{Company}Updater --install`.
The application should then register itself with the updater by calling
`~/Library/Application Support/{Company}/{Company}Updater/Current/{Company}Updater.app/Contents/Helpers/{Company}SoftwareUpdater.bundle/Contents/Helpers/ksadmin -r -P product_id -v version -x path_to_application_bundle -U`.

If the app is running as root, it must add ` --system` to the install command
above, use `-S` instead of `-U` in the ksadmin command, and use the ksadmin in
`/Library` instead of `~/Library`.

Additional [registration arguments](functional_spec#keystone-shims) are
available to register additional data and provide alternative ways to track the
version of a product.

Repeating the updater installation and app registration is not harmful. To
make the system more resilient, apps may periodically repeat the installation
and registration process.

#### CRURegistration library

An Objective-C library to perform these operations is in development. When
available, applications will be able to initialize
[`CRURegistration`](https://chromium.googlesource.com/chromium/src/+/main/chrome/updater/mac/client_lib)
with their product IDs, then use `installUpdaterWithReply:` and
`registerVersion:existenceCheckerPath:serverURLString:reply:` to install the
updater (if needed) and register. These methods operate asynchronously using
[`dispatch/dispatch.h`](https://developer.apple.com/documentation/dispatch/dispatch_queue)
mechanisms. `CRURegistration` maintains an internal task queue, so clients can
call `register...` immediately after `install...` without waiting for a result.

`CRURegistration` uses the helpers and command line binaries documented above.
To install the updater using `CRURegistration`, the updater must be embedded
as a Helper as documented above.

`CRURegistration` is designed to depend only on APIs published in macOS SDKs
and compile as pure Objective-C (without requiring C++ support) so it can be
dropped into projects without incurring Chromium dependencies.

## Uninstalling Applications and the Updater

The updater will uninstall itself automatically when it has no applications to
manage, but it may need some help to do so in a timely manner.

On Windows, when an application is uninstalled, it should delete the
`(HKCU or HKLM)\SOFTWARE\{Company}\Update\Clients\{AppID}` key and then run
the command line in
`(HKCU or HKLM)\SOFTWARE\{Company}\Updater` → `UninstallCmdLine`. This will
notify the updater of the uninstallation.

On macOS, it is assumed that users uninstall software by deleting the app
bundle. The updater will notice this on its own in a few hours. However, it
can be notified earlier by running any
`{Company}Updater.app/Contents/MacOS/{Company}Updater --wake-all` (with
`--system` for system-scope installs).

## Additional Updater Command Lines

Command line arguments for the updater client are documented in the [functional spec](functional_spec.md#Command-Line).

## Error codes

To allow for the updater metainstaller process exit codes to be meaningful, all
metainstaller and updater error codes are in a range above 0xFFFF (65535) for
Windows only, which is the range of Windows error codes.

Specifically:
* [Metainstaller error codes](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/win/installer/exit_code.h)
are in the 73000 range.
* Error codes
[funnelled through `update_client`](https://source.chromium.org/search?q=kCustomInstallErrorBase&sq=&ss=chromium%2Fchromium%2Fsrc:chrome%2Fupdater%2F)
are in the 74000 range.
* [updater error codes](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/constants.h?q=%22%2F%2F%20Error%20codes.%22&ss=chromium%2Fchromium%2Fsrc:chrome%2Fupdater%2F)
are in the 75000 range.

## Dynamic Install Parameters

Windows tagged metainstallers support a number of dynamic install parameters:

### `needsadmin`

`needsadmin` is one of the install parameters that can be specified for
first installs via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).
`needsadmin` is used to indicate whether the application needs admin rights to
install.

For example, here is a command line for the Updater on Windows that includes:
```
UpdaterSetup.exe --install="appguid=YourAppID&needsadmin=False"
```

In this case, the updater client understands that the application installer
needs to install the application on a per-user basis for the current user.

`needsadmin` has the following supported values:
* `true`: the application supports being installed systemwide and once
installed, is available to all users on the system.
* `false`: the application supports only user installs.
* `prefers`: the application installation is first attempted systemwide. If the
user refuses the
[UAC prompt](https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works)
however, the application is then only installed for the current user. The
application installer needs to be able to support the installation as system, or
per-user, or both modes.

### `installdataindex`

`installdataindex` is one of the install parameters that can be specified for
first installs on the command line or via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).

For example, here is a typical command line for the Updater on Windows:
```
UpdaterSetup.exe /install "appguid=YourAppID&appname=YourAppName&needsadmin=False&lang=en&installdataindex =verboselog"
```

In this case, the updater client sends the `installdataindex` of `verboselog` to
the update server.

The server retrieves the data corresponding to `installdataindex=verboselog` and
returns it back to the updater client.

The updater client writes this data to a temporary file in the same directory as
the application installer.

The updater client provides the temporary file as a parameter to the application
installer.

Let's say, as shown above, that the update server responds with these example
file contents:
```
{"logging":{"verbose":true}}
```

The updater client will now create a temporary file, say `c:\my
path\temporaryfile.dat` (assuming the application installer is running from
`c:\my path\YesExe.exe`), with the following file contents:
```
\xEF\xBB\xBF{"logging":{"verbose":true}}
```

and then provide the file as a parameter to the application installer:
```
"c:\my path\YesExe.exe" --installerdata="c:\my path\temporaryfile.dat"
```

* Notice above that the temp file contents are prefixed with an UTF-8 Byte Order
Mark of `EF BB BF`.
* For MSI installers, a property will passed to the installer:
`INSTALLERDATA="pathtofile"`.
* For exe-based installers, as shown above, a command line parameter will be
passed to the installer: `--installerdata="pathtofile"`.
* For Mac installers, an environment variable will be set:
`INSTALLERDATA="pathtofile"`.
* Ownership of the temp file is the responsibility of the application installer.
The updater will not delete this file.
* This installerdata is not persisted anywhere else, and it is not sent as a
part of pings to the update server.

## Application Commands

The Application Command feature allows installed Updater-managed products to
pre-register and then later run command lines (elevated for system
applications). The command lines can also include replaceable parameters
substituted at runtime.

For more information, please see the
[functional spec](functional_spec.md#Application-Commands).

## Logging & Debugging

The updater writes logs to its product directory, whether or not it is
installed. Note that there are two possible product directories (one for
system scope and one for user scope), and so there are often two log files.

See [the functional spec](functional_spec.md#logging) for more details.
