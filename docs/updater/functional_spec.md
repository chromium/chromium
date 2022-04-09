# Chromium Updater Functional Specification

This is the functional specification for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).
It describes the externally observable behavior of the updater, including APIs
and UI.


[TOC]

## Metainstaller
The metainstaller (UpdaterSetup) is a thin executable that contains a compressed
copy of the updater as a resource, extracts it, and triggers installation of the
updater / an app. The metainstaller is downloaded by the user and can be run
from any directory.

TODO(crbug.com/1035895): Document tagging.

### Elevation (Windows)
The metainstaller parses its tag and re-launches itself at high integrity if
installing an application with `needsadmin=true` or `needsadmin=prefers`.

## Standalone Installer
TODO(crbug.com/1035895): Document the standalone installer.

TODO(crbug.com/1035895): Document bundled installers.

TODO(crbug.com/1035895): Document bundling the updater on macOS.

## Updater
The updater is installed at:
*   (Windows, User): `%LOCAL_APP_DATA%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
*   (Windows, System): `%PROGRAM_FILES%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
*   (macOS, User): `~/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`
*   (macOS, System): `/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`

The updater's functionality is split between several processes. The mode of a
process is determined by command-line arguments:
*   --install [--app-id=...]
    *   Install and activate this version of the updater if there is no active
        updater.
    *   --app-id=...
        *   Also install the given application.
    *   --tag=...
        *   Supplies the install metadata needed when installing an
            application. Typically, a tagged metainstaller invokes the updater
            with this command line argument.
        *   If --tag is specified, --install is assumed.
    *   --handoff=...
        *   As --tag.
    *   --install-from-out-dir
        *   TODO(crbug.com/1035895): Document
*   --uninstall
    *   Uninstall all versions of the updater.
*   --uninstall-self
    *   Uninstall this version of the updater.
*   --uninstall-if-unused
    *   Uninstall all versions of the updater, only if there are no apps being
        kept up to date by the updater.
*   --wake
    *   Trigger the updater's periodic background tasks. If this version of the
        updater is inactive, it may qualify and activate, or uninstall itself.
        If this version of the updater is active, it may check for updates for
        applications, unregister uninstalled applications, and more.
*   --crash-me
    *   Record a backtrace in the log, crash the program, save a crash dump,
        and report the crash.
*   --crash-handler
    *   Starts a crash handler for the parent process.
*   --server
    *   Launch the updater RPC server. The server will answer RPC messages on
        the UpdateService interface only.
    *   --service=update|update-internal
        *   If `update`, the server will answer RPC messages on the
            UpdateService interface only.
        *   If `update-internal`, the server will answer RPC messages on the
            UpdateServiceInternal interface only.
*   --windows-service
    *   This switch starts the Windows service. This switch is invoked by the
        SCM either as a part of system startup (`SERVICE_AUTO_START`) or when
        `CoCreate` is called on one of several CLSIDs that the server supports.
    *   --console
        *   Run in interactive mode.
    *   -–com-service
        *   If present, run in a mode analogous to --server --service=update.
            This switch is passed to `ServiceMain` by the SCM when CoCreate is
            called on one of several CLSIDs that the server supports. This is
            used for:
            *   The Server for the UI when installing Machine applications.
            *   The On-Demand COM Server for Machine applications.
            *   COM Server for launching processes at System Integrity, i.e.,
                an Elevator.
*   --update
    *   Install this version of the updater as an inactive instance.
*   --recover
    *   Repair the installation of the updater.
    *   --appguid=...
        *   After recovery, register an application with this id.
    *   --browser-version=...
        *   Register an application with this version.
        *   If --browser-version is specified, --recover can be omitted.
    *  --sessionid=...
        *   Specifies the sesionid associated with this recovery attempt.
*   --test
    *   Exit immediately with no error.
*   --healthcheck
    *   Exit immediately with no error.

If none of the above arguments are set, the updater will exit with an error.

Additionally, the mode may be modified by combining it with:
*   --system
    *   The updater operates in system scope if and only if this switch is
        present.

### Installation
TODO(crbug.com/1035895): Document UI/UX

TODO(crbug.com/1035895): Document installer APIs

TODO(crbug.com/1035895): Document shim installation

TODO(crbug.com/1035895): Document handoff

TODO(crbug.com/1035895): Document relevant enterprise policies.

#### Dynamic Install Parameters

##### `needsadmin`

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

##### `installdataindex`

`installdataindex` is one of the install parameters that can be specified for
first installs on the command line or via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).

For example, here is a typical command line for the Updater on Windows:
```
UpdaterSetup.exe /install "appguid=YourAppID&appname=YourAppName&needsadmin=False&lang=en&installdataindex =verboselog"
```

In this case, the updater client sends the `installdataindex` of `verboselog` to
the update server.

This is how a [JSON](https://www.json.org/) request from the updater client may
look like:

```
{
   "request":{
      "@os":"win",
      "@updater":"updater",
      "acceptformat":"crx3",
      "app":[
         {
            "appid":"YourAppID",
            "data":[
               {
                  "index":"verboselog",
                  "name":"install"
               }
            ],
            "enabled":true,
            "installsource":"ondemand",
            "ping":{
               "r":-2
            },
            "updatecheck":{
               "sameversionupdate":true
            },
            "version":"0.1"
         }
      ],
      "arch":"x86",
      "dedup":"cr",
      "domainjoined":true,
      "hw":{
         "avx":true,
         "physmemory":32,
         "sse":true,
         "sse2":true,
         "sse3":true,
         "sse41":true,
         "sse42":true,
         "ssse3":true
      },
      "ismachine":false,
      "lang":"en-US",
      "nacl_arch":"x86-64",
      "os":{
         "arch":"x86_64",
         "platform":"Windows",
         "version":"10.0.19042.1586"
      },
      "prodversion":"101.0.4949.0",
      "protocol":"3.1",
      "requestid":"{6b417770-1f68-4d52-8843-356760c84d33}",
      "sessionid":"{37775211-4487-48d5-845d-35a1d71b03bc}",
      "updaterversion":"101.0.4949.0",
      "wow64":true
   }
}
```

The server retrieves the data corresponding to `installdataindex=verboselog`
and returns it back to the updater client.

This is how a JSON response from the update server may look like:

```
  "response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "data":[{
      "status":"ok",
      "name":"install",
      "index":"verboselog",
      "#text":"{\"logging\":{\"verbose\":true}}"
     }],
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "run":"UpdaterSetup.exe",
      "arguments":"--arg1 --arg2",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }
```

The updater client writes this data to a temporary file in the same directory as
the application installer. This is for security reasons, since writing the data
to the temp directory could potentially allow a man-in-the-middle attack.

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

## Updates
TODO(crbug.com/1035895): Document server API (Omaha Protocol).

TODO(crbug.com/1035895): Document supported update formats.

TODO(crbug.com/1035895): Document differential updates.

TODO(crbug.com/1035895): Document update timing.

TODO(crbug.com/1035895): Document on-demand APIs.

TODO(crbug.com/1035895): Document registration APIs.

TODO(crbug.com/1035895): Document activity API.

TODO(crbug.com/1035895): Document EULA signals.

TODO(crbug.com/1035895): Document usage-stats opt-in signals.

TODO(crbug.com/1035895): Document relevant enterprise policies.

### Telemetry
When the updater installs an application (an installer is run) it will send an
event with `"eventtype": 2` indicating the outcome of installation. The updater
does not send such a ping for its own installation.

When the updater updates an application (including itself) it will send an
event with `"eventtype": 3` indicating the outcome of update operation.

## Services
TODO(crbug.com/1035895): Document app commands.

TODO(crbug.com/1035895): Document updater crash reporting.

## Uninstallation
TODO(crbug.com/1035895): Document uninstallation APIs.

TODO(crbug.com/1035895): Document updater self-uninstallation.

## Associated Tools
TODO(crbug.com/1035895): Document external constant overrides (test build only).

TODO(crbug.com/1035895): Document tagging tools.
