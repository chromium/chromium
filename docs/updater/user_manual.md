# Chromium Updater User Manual

This is the user manual for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).

[TOC]

## Error codes

The updater setup process can exit with the following error codes:
* UNABLE_TO_ELEVATE_METAINSTALLER = 113: This error code indicates that the
updater setup failed to elevate itself when trying to install a system app.

## Dynamic Install Parameters

### `needsadmin`

`needsadmin` is one of the install parameters that can be specified for
first installs via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).
`needsadmin` is used to indicate whether the application needs admin rights to
install.

For example, here is a command line for the Updater on Windows that includes:
```
UpdaterSetup.exe --install --tag="appguid=YourAppID&needsadmin=False"
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
