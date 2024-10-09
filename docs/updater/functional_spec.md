# Chromium Updater Functional Specification

This is the functional specification for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).
It describes the externally observable behavior of the updater, including APIs
and UI.


[TOC]

## Installation

### Metainstaller
The metainstaller (UpdaterSetup) is a small executable that contains a
compressed copy of the updater as a resource, extracts it, and triggers
installation of the updater / an app. The metainstaller is downloaded by the
user and can be run from any directory.

The metainstaller may have a tag attached to it. The tag is a piece of unsigned
data from which the metainstaller extracts the ID of the application to be
installed, along with the application's brand code, usage-stats opt-in status,
and any additional parameters to be associated with the application.

After the metainstaller installs the updater, the updater installs an
application by connecting to update servers and [downloading and executing an
application installer](#Updates).

On Windows, the tag is embedded in one of the certificates in the metainstaller
PE. The tag is supported for both EXE and MSI formats.

#### Tag Format
Tags have a format of a UTF-8 string `Gact2.0Omaha{length}{tag}`, where
`{length}` is a big-endian uint16, and `{tag}` is `{length}` bytes long.

The format of the `{tag}` piece is further documented in
[tag.h](https://chromium.googlesource.com/chromium/src/+/main/chrome/updater/tag.h#159).

The project also contains code used by non-Google embedders to support UTF-16
tags of the format `Gact2.0Omaha{tag}ahamO0.2tcaG`, but Chromium-branded and
Google-branded builds assume the first case.

##### Brand code
The brand code is a string of arbitrary length. The brand code is persisted
during the first install of the app. Over-installs and updates do not modify
the brand code.

Note: the limit used to be 4 characters in the previous implementation of the
updater.

On macOS, the brand code (as well as AP parameter and the app version) can be
specified using a path to a plist file and a key within that plist file. When
so specified, the updater will read the associated value from the plist and use
it rather than the updater's built-in `pv` value. This allows the updater to
detect and properly represent any overinstallations of an application, which are
done by users or third-party software on macOS (and don't otherwise interact
with the updater).

#### Elevation (Windows)
The metainstaller parses its tag and re-launches itself at high integrity if
it is being run at medium integrity with UAC on and installing an application
with `needsadmin=true` or `needsadmin=prefers`.

More information is in the
[design document](design_doc.md#elevation)
.

#### De-elevation (Windows)
The metainstaller parses its tag and re-launches itself at medium integrity if
it is being run at high integrity with UAC on and installing an application with
`needsadmin=false`.

More information is in the
[design document](design_doc.md#de_elevation)
.

#### Localization
Metainstaller localization presents the metainstaller UI with the user's
preferred language on the current system. Every string shown in the UI is
translated.

### Bundle Installer
TODO(crbug.com/40664480): Implement bundle installers.

The bundle installer allows installation of more than one application. The
bundle installer is typically used in software distribution scenarios.

### Offline/Standalone Installer
Offline installers embed all data required to install the application,
including the payload and various configuration data needed by the application
setup. Such an install completes even if a network connection is not available.

Offline installers are used:

1. when an interactive user experience is not needed, such as automated
deployments in an enterprise.
2. when downloading the application payload is not desirable for any reason.
3. during OEM installation.

On Windows, a offline installer is created by embedding a manifest file and
application installer inside of the metainstaller (UpdaterSetup). These files
are embedded in the metainstaller's `updater.7z` archive at the following paths
using
[sign.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/win/signing/sign.py)
:
* `bin\Offline\{GUID}\OfflineManifest.gup`
* `bin\Offline\{GUID}\{app_id}\installer.exe`

`{GUID}` is an unique Windows GUID in the format
`{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`.

See the
[Steps to create an offline metainstaller for Windows](#steps-to-create-an-offline-metainstaller-for-windows)
section for how to create an example offline installer.

See the [Offline installs](#offline-installs) section below for more information
on the manifest file and application installer.

Applications on macOS frequently install via "drag-install", and then install
the updater using an offline installer on the application's first-run. The
updater app can be embedded in a macOS application bundle as a helper and then
invoked with appropriate command line arguments to install itself.

##### Steps to create an offline metainstaller for Windows

An offline metainstaller can be created using the
[sign.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/win/signing/sign.py)
tool, and can then be tagged by using the metainstaller tagging tool
[tag.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).

The example below generates an offline installer for `Chrome Beta`, with an
`appid` of `{8237E44A-0054-442C-B6B6-EA0509993955}`.

The input files are the following:
* the untagged metainstaller `out\ChromeBrandedDebug\UpdaterSetup.exe`.
* the setup executable for Chrome Beta, saved at:
`out/ChromeBrandedDebug/UpdaterSigning/chrome_installer.exe`.
* the manifest, shown below, saved at:
`out/ChromeBrandedDebug/UpdaterSigning/OfflineManifest.gup`.
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0">
  <systemrequirements platform="win" arch="${ARCH_REQUIREMENT}" min_os_version="6.1"/>
  <app appid="${APP_ID}" status="ok">
    <updatecheck status="ok">
      <urls>
        <url codebase="https://dl.google.com/edgedl/chrome/install/${INSTALLER_VERSION}/"/>
      </urls>
      <manifest version="${INSTALLER_VERSION}">
        <packages>
          <package name="${INSTALLER_FILENAME}" hash_sha256="${INSTALLER_HASH_SHA256}" size="${INSTALLER_SIZE}" required="true"/>
        </packages>
        <actions>
          <action event="install" run="${INSTALLER_FILENAME}" arguments="--do-not-launch-chrome" needsadmin="false" />
          <action event="postinstall" onsuccess="exitsilentlyonlaunchcmd"/>
        </actions>
      </manifest>
    </updatecheck>
  </app>
</response>
```

* and the final tagged offline setup file will be
`out\ChromeBrandedDebug\UpdaterSigning\Signed_ChromeBetaOfflineSetup.exe`.

Here are the steps to create a tagged offline metainstaller, with the `out`
directory at `out\ChromeBrandedDebug`:

* One-time step: from an elevated powershell prompt:
```
New-SelfSignedCertificate -DnsName id@domain.tld -Type CodeSigning
 -CertStoreLocation cert:\CurrentUser\My
```
* Note: **all the steps below are run from a medium cmd prompt.**
* One-time step: `python3 -m pip install pypiwin32`
* One-time step:
```
set PYTHONPATH=C:\src\chromium\src\chrome\tools\build\win`
```
* create the untagged offline installer `ChromeBetaOfflineSetup.exe`:
```
python3 chrome/updater/win/signing/sign.py                                     ^
  --in_file out/ChromeBrandedDebug/UpdaterSetup.exe                            ^
  --out_file out/ChromeBrandedDebug/UpdaterSigning/ChromeBetaOfflineSetup.exe  ^
  --identity id@domain.tld                                                     ^
  --appid {8237E44A-0054-442C-B6B6-EA0509993955}                               ^
  --installer_path out/ChromeBrandedDebug/UpdaterSigning/chrome_installer.exe  ^
  --manifest_path out/ChromeBrandedDebug/UpdaterSigning/OfflineManifest.gup    ^
  --lzma_7z "C:/Program Files/7-Zip/7z.exe"                                    ^
  --signtool ../../fig/google3/third_party/windows_sdk/windows_sdk_10/files/bin/10.0.22000.0/x86/signtool.exe ^
  --tagging_exe out/ChromeBrandedDebug/UpdaterSigning/tag.exe  ^
  --manifest_dict_replacements "{'${INSTALLER_VERSION}':'110.0.5478.0', '${ARCH_REQUIREMENT}':'x86'}"
```
* tag the offline installer and save the result as
`Signed_ChromeBetaOfflineSetup.exe`:
```
out/ChromeBrandedDebug/UpdaterSigning/tag.exe                                                                   ^
  --set-tag="appguid={8237E44A-0054-442C-B6B6-EA0509993955}&appname=Google%20Chrome%20Beta&needsadmin=Prefers"  ^
  --out=out/ChromeBrandedDebug/UpdaterSigning/Signed_ChromeBetaOfflineSetup.exe                                 ^
  out/ChromeBrandedDebug/UpdaterSigning/ChromeBetaOfflineSetup.exe
```
* Now you can run the final signed offline installer:
`Signed_ChromeBetaOfflineSetup.exe`!

In the example above, the `OfflineManifest.gup` has the following replaceable parameters which are replaced by `sign.py` as follows:
* `${APP_ID}`: `{8237E44A-0054-442C-B6B6-EA0509993955}`.
* `${INSTALLER_FILENAME}`: `chrome_installer.exe`.
* `${INSTALLER_SIZE}`: computed size of `installer_path`.
* `${INSTALLER_HASH_SHA256}`: computed sha256 hash of `installer_path`.
* `${INSTALLER_VERSION}`: `110.0.5478.0` (provided on the command line in `--manifest_dict_replacements`).
* `${ARCH_REQUIREMENT}`: `x86` (provided on the command line in `--manifest_dict_replacements`).

The final manifest looks as follows:
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0">
  <systemrequirements platform="win" arch="x86" min_os_version="6.1"/>
  <app appid="{8237E44A-0054-442C-B6B6-EA0509993955}" status="ok">
    <updatecheck status="ok">
      <urls>
        <url codebase="https://dl.google.com/edgedl/chrome/install/110.0.5478.0/"/>
      </urls>
      <manifest version="110.0.5478.0">
        <packages>
          <package name="chrome_installer.exe" hash_sha256="1d28b3ca401b063b3e06b281f8ba80b52797e40de7830f1532c1018544027af8" size="89150504" required="true"/>
        </packages>
        <actions>
          <action event="install" run="chrome_installer.exe" arguments="--do-not-launch-chrome" needsadmin="false" />
          <action event="postinstall" onsuccess="exitsilentlyonlaunchcmd"/>
        </actions>
      </manifest>
    </updatecheck>
  </app>
</response>
```

### MSI Wrapper
TODO(crbug.com/40841203) - Implement and document.

### Scope
The updater is installed in one of the following modes (or scopes):
1. per-system (or per-machine). This mode requires administrator privileges.
2. per-user

Per-machine and per-user instances of the updater can run side by side.

Depending on the scope, the updater is installed at:

* (Windows, User): `%LOCAL_APP_DATA%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
* (Windows, System): `%PROGRAM_FILES%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
* (macOS, User): `~/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`
* (macOS, System): `/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`
* (Linux, User): `~/.local/{COMPANY}/{UPDATERNAME}/{VERSION}/updater`
* (Linux, System): `/opt/{COMPANY}/{UPDATERNAME}/{VERSION}/updater`

### Command Line

The updater's functionality is split between several processes. The mode of a
process is determined by command-line arguments:

*   --install[=tag] [--app-id=...]
    *   Install and activate this version of the updater if there is no active
        updater.
        *   If a tag argument is specified instead of `--app-id`, this supplies
            the install metadata needed when installing an application.
            Typically, a tagged metainstaller invokes the updater with the tag.
            For example: `--install="appguid=foo&appname=Foo&needsadmin=False"`.
    *   --app-id=...
        *   Also install the given application.
    *   --handoff[=tag]
        *   A tag argument can be specified, similar to `--install`.
    *   --appargs="appguid=...&installerdata=..."
        * Allows extra data (`installerdata`) to be communicated to the
          application installer. One per application.
        * `appguid` must be the first arg followed by the `installerdata`. The
          same `appguid` must appear in `--install=` or `--handoff=`.
        * This is an alternative to using `installdataindex` in the tag. Where
          `installdataindex` selects from a pre-defined set of `installerdata`
          options, this value specifies the exact `installerdata` to use.
        * The value of `installerdata` needs to be URL encoded.
        * The data will be decoded and written to a file same as in
          [installdataindex](#installdataindex).
    *   --offlinedir={GUID}
        *   Performs offline install, which means no update check or file
            download is performed against the server during installation.
            All data is read from the files in the offline directory instead.
        *   The following are the files in the offline directory, which is at
            `{CURRENT_PROCESS_DIR}\Offline\{GUID}`:
            * Manifest file, named `OfflineManifest.gup` or *`<app-id>`*`.gup`.
              The file contains the update check response in XML format.
            * {AppId}\AppInstaller.exe/msi.
            * See the "Offline installs" section below for more information.
        *   The switch can be combined with `--handoff` above.
        *   --enterprise
            *   Suppresses transmission of pings from the offline install.
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
*   --wake-all
    *   Runs --wake for every updater version installed in this scope.
*   --crash-me
    *   Record a backtrace in the log, crash the program, save a crash dump,
        and report the crash.
*   --crash-handler
    *   Starts a crash handler for the parent process.
*   --server
    *   Launch the updater RPC server. The server answers RPC messages on
        the UpdateService interface only.
    *   --service=update|update-internal
        *   If `update`, the server answers RPC messages on the
            UpdateService interface only.
        *   If `update-internal`, the server answers RPC messages on the
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

If none of the above arguments are set, the updater exits with an error.

Additionally, the mode may be modified by combining it with:
*   --system
    *   The updater operates in system scope if and only if this switch is
        present.

### Backward-Compatible Updater Shims
To maintain backwards compatibility with
[Omaha](https://github.com/google/omaha) and
[Keystone](https://code.google.com/archive/p/update-engine/), the updater
installs small versions of those programs that implement a subset of their APIs.

The updater also imports the properties and state of the apps that have been
registered with Omaha and Keystone, so they show up as registered with the
updater. This import is repeated periodically, so long as the updater is
installed, but these properties do not override any existing properties the
updater already tracks for each app.

#### Keystone Shims
The updater installs a Keystone-like application that contains these shims:

1.  The Keystone app executable.
1.  The ksadmin helper executable.
2.  The ksinstall helper executable.
3.  The Keystone Agent helper app executable.

Both the Keystone and Keystone Agent executables simply exit immediately when
run.

The ksinstall executable expects to be called with `--uninstall` and possibly
`--system`. If so, it deletes the Keystone shim (but not the overall updater)
from the file system. Otherwise, it exits with a non-zero exit code.

The ksadmin shim is frequently called by applications and handles a variety of
command lines:

*   --delete, -d
    *   Delete a ticket.
    *   Accepts -P.
*   --install, -i
    *   Check for and apply updates.
*   --ksadmin-version, -k
    *   Print the version of ksadmin.
*   --print
    *   An alias for --print-tickets.
*   --print-tag, -G
    *   Print a ticket's tag.
    *   Accepts -P.
*   --print-tickets, -p
    *   Print all tickets.
    *   Accepts -P.
*   --register, -r
    *   Register a new ticket or update an existing one.
    *   Accepts -P, -v, -x, -e, -a, -K, -H, -g.

Some of these actions accept parameters:

*   --brand-key, -b plistKeyName
    *   Set the brand code key. Use with -P and -B. Value must be empty or
        KSBrandID.
*   --brand-path, -B pathToPlist
    *   Set the brand code path. Use with -P and -b.
*   --productid, -P id
    *   Specifies the application ID.
*   --system-store, -S
    *   Use the system-scope updater, even if not running as root.
    *   Not all operations can be performed with -S if not running as root.
*   --tag, -g ap
    *   Set the application's additional parameters. Use with -P.
*   --tag-key, -K plistKeyName
    *   Set the additional parameters path key. Use with -P and -H.
*   --tag-path, -H pathToPlist
    *   Set the tag path. Use with -P and -K.
*   --user-initiated, -F
    *   Set foreground priority for this operation.
*   --user-store, -U
    *   Use a per-user ticket store, even if running as root.
*   --version, -v version
    *   Set the application's version. Use with -P.
*   --version-key, -e plistKeyName
    *   Set the version path key. Use with -P and -a.
*   --version-path, -a pathToPlist
    *   Set the version path. Use with -P and -e.
*   --xcpath, -x PATH
    *   Set a path to use as an existence checker.

#### Omaha Shims
On Windows, the updater replaces Omaha's files with a copy of the updater, and
keeps the Omaha registry entries
(`CLIENTS/{430FD4D0-B729-4F61-AA34-91526481799D}` and
`CLIENTSTATE/{430FD4D0-B729-4F61-AA34-91526481799D}`) up-to-date with the
latest `pv` value. Additionally, the updater replaces the Omaha uninstall
command line with its own.

The updater takes over the COM registration for the following classes:
* GoogleUpdate3WebUserClass
* GoogleUpdate3WebSystemClass
* GoogleUpdate3WebServiceClass
* PolicyStatusUserClass
* PolicyStatusSystemClass
* ProcessLauncherClass

The updater replaces `GoogleUpdate.exe` in the `Google\Update` directory with a
copy of `updater.exe`. This is to allow the updater to handle handoffs from
legacy Omaha installers that invoke `Google\Update\GoogleUpdate.exe`. All other
legacy directories under `Google\Update` are removed.

The updater removes the following Omaha registrations:
* For system installations only: Removes the GoogleUpdate services "gupdate" and
  "gupdatem".
* For user installations only: Removes the GoogleUpdate run value in the
  registry at `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.
* Removes the Omaha Core and UA tasks.

#### Runtime mode (Windows)
Similar to Omaha, the updater supports command lines of the form:
`UpdaterSetup.exe /install "runtime=true"`
`UpdaterSetup.exe /install "runtime=true&needsadmin=false"`
`UpdaterSetup.exe /install "runtime=true&needsadmin=true"`

The "runtime" argument in the tag tells the updater to install itself and stay
on the system without any associated application. The updater will stay on for
at least `kMaxServerStartsBeforeFirstReg` wakes. This feature is used to expose
the COM API to a process that will install applications via that API.

### Installer User Interface
During the initialization of the installer, the user is shown a splash screen UI
briefly before a full-fledged UI is shown. Installer initialization involves
unzipping and unpacking the installer files.

The splash screen logo can be customized by editing
[logo.bmp](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/win/installer/logo.bmp)
.

During installation, the user is presented with a UI that displays the progress
of the download and installation. The user may close the dialog, which cancels
the installation. A cancelled installation still results in an event ping to
the server indicating an installation failure.

The user interface is localized in the same languages as the Chromium project.

No UI will be shown if the `--silent` switch is specified on the command line.

The launch command provided by the application installer via the
[installer result API](#installer-result-api)
will be run unconditionally, even for silent modes, if the `--alwayslaunchcmd`
switch is specified on the command line.

#### Help Button
If the installation fails, the updater shows an error message with a "Help"
button. Clicking the help button opens a web page in the user's default browser.
The page is opened with a query string:
`?product={AppId}&errorcode={ErrorCode}`.

## Updates
There is no limit for the number of retries to update an application if the
update fails repeatedly.

### Protocol
The updater communicates with update servers using the
[Omaha Protocol](protocol_3_1.md).

The updater uses platform-native network stacks (WinHTTP on Windows and
NSURLSession on macOS) with user-agent `"{PRODUCT_FULLNAME} {UpdaterVersion}"`.

#### Security
It is not possible to MITM the updater even if the network (including TLS) is
compromised. The integrity of the client-server communication is guaranteed
by the [Client Update Protocol (CUP)](cup.md).

##### COM Security

The legacy COM classes in updater_legacy_idl.template allow non-admin callers
because the interfaces only expose functionality that non-admin callers need.
One exception is that calling `IAppBundleWeb::install()` after calling
`IAppBundleWeb::createApp` requires the caller to be admin, since it allows
installing a new application.

The new COM classes in updater_internal_idl.template and updater_idl.template
require the callers to be admin. This is because the corresponding interfaces
allow for unrestricted functionality, such as installing any app that the
updater supports. For non-admins, COM creation will fail with E_ACCESSDENIED.

#### Retries
The updater does not retry an update check that transacted with the backend,
even if the response was erroneous (misformatted or unparsable), until the
next normally scheduled update check.

#### DOS Mitigation
The updater sends [DoS mitigation headers](protocol_3_1.md) in requests to the
server.

When the server responds with an `X-Retry-After header`, the client does not
issue another update check until the specified period has passed (maximum 24
hours).

* The updater distinguishes between foreground and background priority: if an
  `X-Retry-After` was received in the background case, a foreground update is
  still permitted (but not if it was received in response to a foreground
  update).

#### Usage Counts
The updater implements [date-last counting](protocol_3_1.md#User-Counting),
allowing servers to anonymously count the number of active updaters and
applications.

#### Cohort Tracking
The client records the `cohort`, `cohortname`, and `cohorthint` values from the
server in each update response (even if there is no-update) and reports them on
subsequent update checks.

#### Install Source and update disposition.
All application installs and user-initiated application updates are processed
as foreground operations and with an `installsource` set to "ondemand".

### Installer Result API
As part of installing or updating an application, the updater executes the
application's installer. The API for the application installer is platform-
specific.

Application installers are run with a 15-minute timeout. If the installer runs
for longer than this, the updater assumes failure and continues operation.
However, the updater does not kill the installer process.

The application installer API varies by platform.
[macOS](installer_api_mac.md),
[Windows](https://chromium.googlesource.com/chromium/src/+/main/chrome/updater/win/installer_api.h).

For Windows, for backward compatibility, the following installer results are
read and written from the registry:

* `InstallerProgress` : The installer writes a percentage value (0-100) while
installing so that the updater can provide feedback to the user on the progress.
* `InstallerError` : Installer error, or 0 for success.
* `InstallerExtraCode1` : Optional extra code.
* `InstallerResult` : Specifies the result type and how to determine success or
failure:
  *   0 - SUCCESS
      The installer succeeded, unconditionally.
      - if a launch command was provided via the installer API, the command will
        be launched and the updater UI will exit silently. Otherwise, the
        updater will show an install success dialog.

  *   All the error installer results below are treated the same.
      - if an installer error was not provided via the installer API or the exit
        code, generic error `kErrorApplicationInstallerFailed` will be reported.
      - the installer extra code is used if reported via the installer API.
      - the text description of the error is used if reported via the installer
        API.
      *   1 - FAILED\_CUSTOM\_ERROR
      *   2 - FAILED\_MSI\_ERROR
      *   3 - FAILED\_SYSTEM\_ERROR
      *   4 - FAILED\_EXIT\_CODE (default)

  *   If an installer result is not explicitly reported by the installer, the
      installer API values are internally set based on whether the exit code
      from the installer process is a success or an error:
      - If the exit code is a success, the installer result is set to success.
        If a launch command was provided via the installer API, the command will
        be launched and the updater UI will exit silently. Otherwise, the
        updater will show an install success dialog.
      - If the exit code is a failure, the installer result is set to
        `kExitCode`, the installer error is set to
        `kErrorApplicationInstallerFailed`, and the installer extra code is set
        to the exit code.
      - If a text description is reported via the installer API, it will be
        used.
* `InstallerResultUIString` : A string to be displayed to the user, if
`InstallerResult` is FAILED*.
* `InstallerSuccessLaunchCmdLine` : On success, the installer writes a command
line to be launched by the updater. The command line will be launched at medium
integrity on Vista with UAC on, even if the application being installed is a
machine application. Since this is a command line, the application path should
be properly enclosed. For example:
`"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe" /foo`

On an update or install, the InstallerXXX values are renamed to LastInstallerXXX
values. The LastInstallerXXX values remain around until the next update or
install. Legacy MSI installers read values such as the
`LastInstallerResultUIString` from the `ClientState` key in the registry and
display the string.

TODO(crbug.com/40229998): Implement running installers at
BELOW_NORMAL_PRIORITY_CLASS if the update flow is a background flow.

#### Updater UI behavior

The updater UI does the following:
*   The title of the UI is derived from the `bundlename` if specified, or
    otherwise the `appname` from the metainstaller `tag`. For instance, if no
    `bundlename` is specified, and the `appname` is "Sample App", the title of
    the UI will be "Sample App Installer". If no `bundlename` or `appname` is
    specified, the UI title will be a generic `Chromium Installer` for
    unbranded, or `Google Installer` for branded.
*   on successful installs that do not specify an installer API launch command:
    *   Displays a "Thank you for installing" message that the user must click
        to close.
*   on successful installs that specify a launch command:
    *   Shows all UI until installation is completed then launches the launch
        command and then exits without displaying the thank you message or
        requiring the user to click on the UI.
*   on error:
    *   The UI is always shown with the error message. If a specific text
        description of the error is provided via the installer API, that is
        shown. Otherwise, a generic message with the error code is shown, either
        the code provided via the installer API, or the exit code, or a generic
        error `kErrorApplicationInstallerFailed`.

### Installer Setup

To maintain backwards compatibility with
[Omaha](https://github.com/google/omaha), the updater setup signals a shutdown
event that Omaha listens to, so that Omaha processes can shut down gracefully.
The updater then proceeds to overinstall the Omaha binaries with the updater
binaries.

#### Serializing Setup in the updater

The updater needs to serialize the following command lines:

* `--install`
* `--uninstall-self`, `--uninstall`, `--uninstall-if-unused`
* `--update`

for each specific version, so that they do not trigger concurrency issues that
lead to an undefined state of the installation such as missing/incorrect/corrupt
files/tasks/services/registration.

Errors may happen, such as a timeout due to not being able to acquire a lock,
but the goals are to prevent corrupt states or a permanent deadlock.

Since the versions are installed SxS, the classes corresponding to the above
command lines, i.e., AppInstall, AppUninstall, and AppUpdate will take a
version-specific setup lock at construction time, and then proceed to
install/uninstall/update that specific version.

All three of --uninstall-self, --uninstall, --uninstall-if-unused take the
version-specific setup lock for that particular version first. --uninstall and
--uninstall-if-unused call --uninstall-self on other versions that take their
own version specific setup locks.

The version-specific setup lock is always acquired first. The setup lock
serializes the installation/uninstallation of files, services, tasks, for that
specific version, and nothing else.

The prefs global lock is a related lock used by the updater to serialize common
access points. For instance, AppInstall calls `GetVersion`, which takes the
prefs lock. The prefs lock is also used when swapping a new version to become
the active updater.

Here is an example flow that may result in an error, but will keep the state
deterministic with the design above:

* --uninstall-if-unused for active version A calls --uninstall-self for inactive
version B.
* At the same time, version B is trying to install itself via -install, and
-install is waiting for `GetVersion`.
* `GetVersion` is waiting on the global prefs lock, because
--uninstall-if-unused is holding the global prefs lock.

In this example flow, the following scenarios may occur:

* `GetVersion` may timeout and fail -install on version B, in which case the
-uninstall-self for version B gets the version-specific setup lock and proceeds
to uninstall. Result: The user gets an error, and retries -install.
* Version B's uninstall may timeout getting the version-specific setup lock,
returning back to version A, and version A proceeds to uninstall itself and
releasing the global prefs lock, which allows version B's -install to proceed.
Result: the user gets a successful -install.
* Version B's uninstall may timeout getting the version-specific setup lock, and
`GetVersion` may also timeout and fail the install on version B. Result: The
user gets an error, and retries the install.

### Offline installs

The updater supports offline installations, for which no update check or file
download is performed against the server during installation. All data is read
from the files in the directory instead.

Offline installs include:
* an offline manifest file, which contains the update check response in XML
  format.
* app installer.

Offline install command line format:
* The offline directory is specified on the command line as a relative path in
the format "/offlinedir {GUID}".
* The actual offline directory is at `{CURRENT_PROCESS_DIR}\Offline\{GUID}`.
* The offline manifest is at
`{CURRENT_PROCESS_DIR}\Offline\{GUID}\OfflineManifest.gup`.
* The installer is at
`{CURRENT_PROCESS_DIR}\Offline\{GUID}\{app_id}\installer.exe`.
  * `installer.exe` may not correspond exactly to the value of the manifest's
  `run` attribute, so the code picks the first file it finds in the
  directory if that is the case.

For online app installs, the update server checks the compatibility between the
application and the host OS that the install is attempted on.

The updater client has equivalent support for offline installs, where no update
server is involved.

The `platform`, `arch`, and `min_os_version` attributes in the offline update
response are used to determine compatibility of the app being installed with the
host OS.

Omaha 3 offline manifests have `arch` as "x64", but the Chromium functions
return "x86_64" as the architecture for amd64. The updater accounts for this by
treating "x64" the same as "x86_64".

For more information, see the
[protocol document](protocol_3_1.md#update-checks-body-update-check-response-objects-update-check-response-3).

### MSI installers

MSI installers package an offline/standalone installer, and can be built using
[msi_from_standalone.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/win/signing/msi_from_standalone.py)

`msi_from_standalone.py` builds an MSI around the supplied standalone installer.
This MSI installer is intended to enable enterprise installation scenarios while
being as close to a normal install as possible.

This method only works for application installers that do not use MSI.

For example, to create `GoogleChromeBetaStandaloneEnterprise.msi` from
`ChromeBetaOfflineSetup.exe`:
```
python3 chrome/updater/win/signing/msi_from_standalone.py
    --candle_path ../third_party/wix/v3_8_1128/files/candle.exe
    --light_path ../third_party/wix/v3_8_1128/files/light.exe
    --product_name "GoogleChromeBeta"
    --product_version 110.0.5478.0
    --appid {8237E44A-0054-442C-B6B6-EA0509993955}
    --product_custom_params "&brand=GCEA"
    --product_uninstaller_additional_args=--force-uninstall
    --product_installer_data "%7B%22distribution%22%3A%7B%22msi%22%3Atrue%7D%7D"
    --standalone_installer_path ChromeBetaOfflineSetup.exe
    --custom_action_dll_path out/Default/msi_custom_action.dll
    --msi_base_name GoogleChromeBetaStandaloneEnterprise
    --enterprise_installer_dir chrome/updater/win/signing
    --company_name "Google"
    --company_full_name "Google LLC"
    --architecture x64
    --output_dir out/Default
```

If this untagged MSI installer is run as-is, it will run the updater
metainstaller with the following parameters:
```
--silent
--install=appguid={8237E44A-0054-442C-B6B6-EA0509993955}&
      appname=GoogleChromeBeta&needsAdmin=True&brand=GCEA
--installsource=enterprisemsi
--appargs=appguid={8237E44A-0054-442C-B6B6-EA0509993955}&
          installerdata=%7B%22distribution%22%3A%7B%22msi%22%3Atrue%7D%7D
```

This MSI can be tagged using `tag.exe` as follows:
```
out\ChromeBrandedDebug\tag.exe
    "--set-tag=appguid={8237E44A-0054-442C-B6B6-EA0509993955}&
     appname=Google%20Chrome%20Beta&needsAdmin=True&brand=GGLL"
    GoogleChromeBetaStandaloneEnterprise.msi
```

Notice that the tag overrode the `product_name` and `product_custom_params` that
were used to create the original MSI installer. The tag needs to include the
`appguid`, the `appname`, and `needsAdmin`. Other tag parameters are optional.

If this tagged MSI installer is run, it will run the updater metainstaller with
the following parameters:
```
--silent
--install=appguid={8237E44A-0054-442C-B6B6-EA0509993955}&
      appname=Google%20Chrome%20Beta&needsAdmin=True&brand=GGLL
--installsource enterprisemsi
--appargs=appguid={8237E44A-0054-442C-B6B6-EA0509993955}&
          installerdata=%7B%22distribution%22%3A%7B%22msi%22%3Atrue%7D%7D
```

### Enterprise Enrollment
The machine updater may be enrolled with a particular enterprise. Enrollment is
coordinated with a device management server by means of an enrollment token and
a device management token. The enrollment token is placed on the device by other
programs or the enterprise administrator and serves as an indicator of which
enterprise the device should attempt to enroll with. On Windows platform,
alternatively, an enrollment token can be tagged to the meta-installer by the
key `etoken`. This is called runtime enrollment token and must be a GUID string.
When the meta-installer runs, the tagged token is persisted to
`CloudManagementEnrollmentToken` under registry key
`{CLIENTSTATE}\{UpdaterAppID}`.  The updater searches the enrollment token from
known places in order, sends it along with the device's machine name, os
information, and (on Windows) BIOS serial number. If the server accepts the
enrollment, it responds with a device-specific device management token, which is
used in future requests to fetch device-specific policies from the device
management server.

By default, if enrollment fails, for example if the enrollment token is invalid
or revoked, the updater will start in an unmanaged state. Instead, if you want
to prevent the updater from starting if enrollment fails, set
`EnrollmentMandatory` to `1` (Windows only).

After the updater sets itself up, the `FetchPolicies` RPC is invoked on the
updater server to register with device management and fetch policies. Concurrent
calls of `FetchPolicies` will result in only a single policy fetch.

The updater also checks for policy updates when the `RunPeriodicTasks` RPC is
invoked at periodic intervals.

The maximum size of the token is 4K (Windows only).

#### Windows
The enrollment token is searched in the order:

* The `EnrollmentToken` REG_SZ value from
  `HKLM\Software\Policies\{COMPANY_SHORTNAME}\CloudManagement`
* The `CloudManagementEnrollmentToken` REG_SZ value from
  `HKLM\Software\Policies\{COMPANY_SHORTNAME}\{BROWSER_NAME}`
* The `CloudManagementEnrollmentToken` REG_SZ value from
  `{CLIENTSTATE}\{UpdaterAppID}` (the runtime enrollmen token)
* The `CloudManagementEnrollmentToken` REG_SZ value from
  `{CLIENTSTATE}\{430FD4D0-B729-4F61-AA34-91526481799D}` (the legacy runtime
  enrollment token)

The `EnrollmentMandatory` `REG_DWORD` value is also read from
`HKLM\Software\Policies\{COMPANY_SHORTNAME}\CloudManagement`.

#### macOS
The enrollment token is searched in the order:

* Managed Preferences value with key `CloudManagementEnrollmentToken` in domain
 `{MAC_BROWSER_BUNDLE_IDENTIFIER}`.
* Managed Preferences value with key `EnrollmentToken` in domain
 `{MAC_BROWSER_BUNDLE_IDENTIFIER}`.
* File
 `/Library/{COMPANY_SHORTNAME}/{BROWSER_NAME}/CloudManagementEnrollmentToken`.

CBCM enterprise enrollment and policy fetches are done every time an install or
or update happens, as well as when the updater periodic background task
`--wake` runs.

#### Linux
The enrollment token is stored in:
`/opt/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}/CloudManagementEnrollmentToken`

The device management token is stored in:
`/opt/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}/CloudManagement`

### Enterprise DM token
DM server sends back a DM token to the client device after the device
enrollment. The client persists the DM token for the authorization purpose
in the subsequent communication with the DM server.

DM token is stored at:
##### Windows
- The `dmtoken` REG_BINARY value at path:
  `HKLM\Software\WOW6432Node\{COMPANY_SHORTNAME}\Enrollment\`
- The `dmtoken` REG_BINARY value at path:
  `HKLM64\Software\{COMPANY_SHORTNAME}\{BROWSER_NAME}\Enrollment\`. This is
  for backward compatibility.

#### macOS
- File `/Library/Application Support/{COMPANY_SHORTNAME}/CloudManagement`.

#### Linux
- File `/opt/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}/CloudManagement`.

DM server can send back a response to delete the DM token or invalidate the DM
token during policy fetch. If a DM token is deleted, the device could be
re-enrolled into cloud management at the next `--wake` run provided there is a
valid enrollment token. If a DM token is invalidated, a special DM token value
`INVALID_DM_TOKEN` is persisted at the DM token location. The device won't
re-enroll until the invalidated token is deleted externally.

Note the device must have a valid DM token for the downloaded CBCM policies to
be effective.

### Enterprise Policies
Enterprise policies can prevent the installation of applications:

* A per-application setting may specify whether an application is installable.
* If no per-application setting specifies otherwise, the default install
  policy is used.
* If the default install policy is unset, the application may be installed.

Enterprise policies can control the updates of applications:

* Update policy can be set to be always enabled, automatic updates only, manual
  updates only or disabled.
* Update policy can be set per-application.
* If no per-application setting specifies otherwise, the default update
  policy is used.
* If the default update policy is unset, the application may be updated.
* Updates and qualification are always enabled for the updater itself and can't
  be disabled by policy.
* If the update check period is set to zero, the updater is qualified without
  an update check.

Refer to chrome/updater/protos/omaha\_settings.proto for more details.

Policies may be set by platform-specific means (Group Policy on Windows, Managed
Preferences on macOS), or by communication with the device management server.

For device management, the enterprise policies for Google applications are
downloaded from the device management server periodically and stored at a fixed
secure location. The path on Windows is
`%ProgramFiles(x86)%\Google\Policies` and on macOS is
`/Library/Google/GoogleSoftwareUpdate/DeviceManagement`.

The policy service searches all active policy providers in pre-determined order
for any policy value. When a policy value is configured in multiple providers,
the service always returns the first active valid value.

The policy searching order:
##### Windows
* Policy dictionary defined in
 [External constants](#external-constants-overrides)(testing overrides)
* Group Policy
* Device Management policy (cloud policy)
* Policy from default value provider
>**_NOTE:_** If the global policy `CloudPolicyOverridesPlatformPolicy` is set
to a non-zero DWORD value, then the search order of `Group Policy` and
`Device Management policy` is reversed.


##### macOS
* Policy dictionary defined in
 [External constants](#external-constants-overrides)(testing overrides)
* Device management policy (cloud policy)
* Policy from Managed Preferences
* Policy from default value provider

#### COM interfaces (Windows only)
The updater exposes
[IPolicyStatus4](https://source.chromium.org/search?q=IPolicyStatus4%20file:updater_legacy_idl.template)
and the corresponding `IDispatch` implementation to provide clients such as
Chrome the ability to query the updater enterprise policies.

A client can `CoCreateInstance` the `PolicyStatusUserClass` or the
`PolicyStatusSystemClass` to get the corresponding policy status object and
query it via the `IPolicyStatus4` methods.

#### Enterprise policies ADM/ADMX files (Windows, Google-branded builds only)

ADM/ADMX files for enterprise policies are generated with each build in
`GoogleUpdateAdmx.zip` and `GoogleCloudManagementAdmx.zip` for enterprise
customers.

These ADM/ADMX files are generated using the scripts in
[chrome/updater/enterprise/win/google/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/enterprise/win/google/).

#### Deploying enterprise applications via updater policy
For each application that needs to be deployed via the updater, the policy for
that application can be set to either `Force installs (system wide)` or `Force
installs (per user)`.

The updater then downloads and installs the application on all machines where
the policy is deployed, and where the application is not already installed.

#### CBCM policy cache
The updater fetches all machine level app CBCM policies and caches them in the
file system.  The cached policy files are global readable for other apps to
consume. Location of the policy cache folder:

* **Windows**: `%PROGRAMFILESX86%\{COMPANY_SHORTNAME}\Policies`
* **macOS**: `/Library/{COMPANY_SHORTNAME}/GoogleSoftwareUpdate/DeviceManagement`
* **Linux**: `/opt/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}/DeviceManagement`

The policies are signed. The verification chain is:

* A special file called `CachedPolicyInfo` contains a public signing key
  with its verification data. This public key verification data is signed by
  the pinned key always using `RSA_PKCS1_SHA256`.
* Each type of policy is saved at
  `base64_encoding{policy_type}/PolicyFetchResponse`. This file is signed by
  the key in `CachedPolicyInfo` using the algorithm specified in this file.

The cached policies are cleared:

* before the device enrollment, to make sure the device management starts
  from a clean state.
* after policy fetch succeeds but receives no new policies. This means all
  existing policies become stale and/or policy cache is in abnormal state that
  fails all validations. Either way it's a good idea to reset the state.

### Dynamic Install Parameters

#### `needsadmin`

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

#### `installdataindex`

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
     "urls":{"url":[{"codebase":"https://example.com/"},
                    {"codebasediff":"https://diff.example.com/"}]},
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

The updater client creates a temporary file, say `c:\my
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
* For MSI installers, a property is passed to the installer:
`INSTALLERDATA="pathtofile"`.
* For exe-based installers, as shown above, a command line parameter is
passed to the installer: `--installerdata="pathtofile"`.
* For Mac installers, an environment variable is set:
`INSTALLERDATA="pathtofile"`.
* Ownership of the temp file is the responsibility of the application installer.
The updater does not delete this file.
* This installerdata is not persisted anywhere else, and it is not sent as a
part of pings to the update server.

#### Application logo shown in the UI

The app logo is expected to be hosted at
`{APP_LOGO_URL}{url escaped app_id_}.bmp`.

If `{url escaped app_id_}.bmp` exists, a logo is shown in the updater UI for
that app install.

For example, if `app_id_` is `{8A69D345-D564-463C-AFF1-A69D9E530F96}`, the
`{url escaped app_id_}.bmp` is `%7b8A69D345-D564-463C-AFF1-A69D9E530F96%7d.bmp`.

`APP_LOGO_URL` is specified in chrome/updater/branding.gni.
[branding.gni](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/branding.gni?q=APP_LOGO_URL)

### Update Formats
The updater accepts updates packaged as CRX₃ files. All files are signed with a
publisher key. The corresponding public key is hardcoded into the updater.

### Differential Updates
TODO(crbug.com/40227383): Implement and document differential update support.

### Update Timing
The updater runs periodic tasks every hour, checking its own status, detecting
application uninstalls, and potentially checking for updates.

The updater has a base update check period of 4.5 hours (though this can be
overridden by policy). Each time the updater runs routine tasks, the update
check is only run if the period has elapsed since the last check.

Since the updater's periodic tasks are run every hour, in practice the update
check period is always rounded up to the nearest hour.

To prevent multiple updaters from synchronizing their update checks (for
example, if a large cohort of machines is powered on at the the same time),
the updater will randomly use a longer update check period (120% of the normal
period) with 10% probability.

The updater will always check for updates if the time since the last check is
negative (e.g. due to clock wander).

Once the updater commits to checking for updates, it will delay the actual
check by a random number of milliseconds up to one minute. This avoids
sychronizing traffic to the first second of each minute (or the first
millisecond of each second).

Background updates can be disabled entirely through policy.

Users can trigger an immediate run of the periodic tasks by calling the
RunPeriodicTasks RPC, even for a system updater.

#### Windows Scheduling of Updates
The update wake task is scheduled using the OS task scheduler.

The time resolution for tasks is 1 minute. The update wake task is set to run 5
minutes after creation. If a task execution is missed, it will run as soon as
the system is able to.

The updater runs the wake task at system startup for system installs, via the
system service, which is set to Automatic Start.

The updater also runs at user login. For system installs, this is done via a
logon trigger on the scheduled task. For user installs, this is done via both
the logon trigger on the scheduled task, as well as the "Run" registry entry in
`HKCU` for redundancy.

### Server Lifetime
The updater's RPC server starts and waits for incoming RPCs. The server
considers itself idle if it has not been processing any RPC in the last ten
seconds. Every five minutes, the updater will check itself for idleness and
shut down if idle.

Additionally, on macOS, after answering at least one RPC, the server will shut
itself down as soon as it becomes idle.

Additionally, on Windows, the updater will shut itself down if all clients
release their references to the server.

### On-Demand Updates
The updater exposes an RPC interface for any user to trigger an update check.
The update can be triggered by any user on the system, even in the system scope.

The caller provides the ID of the item to update, the install data index to
request, the priority, whether a same-version update (repair) is permitted, and
callbacks to monitor the progress and completion of the operation.

The interface provides the version of the update, if an update is available.

Regardless of the normal update check timing, the update check is attempted
immediately.

The RPC function UpdateService::CheckForUpdate discovers if updates are
available. By calling this function, a client can check for updates, retrieve
an update response, inspect the availability of an update but not apply the
update.
See
[chrome/updater/update_service.h](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/update_service.h)
for the `UpdateService` RPC interface definition.

For the Google implementation, the updater must write its version under
`HKEY_LOCAL_MACHINE\SOFTWARE\Google\Update` or
`HKEY_CURRENT_USER\SOFTWARE\Google\Update` as a string value named `version`.
The on-demand update client in Google Chrome depends on this value being
present for backward compatibility reasons with older versions of Omaha.

### App Registration
The updater exposes an RPC interface for users to register an application with
the updater. Unlike on-demand updates, cross-user application registration is
not permitted.

If the application is already installed, it is registered with the version
present on disk. If it has not yet been installed, a version of `0` is used.

User-scope updaters will update any application registered with them, except
apps that are managed by a system-scope updater.

System-scope updaters will update any application registered with them. On
POSIX platforms, they will additionally lchown the existence checker path
registered by the application to be owned by the root user. User-scope updaters
use this as a signal that the application is managed by a system-scope updater.

#### Windows

Application installers are expected to register with the updater by setting
[HKCU or HKLM]\SOFTWARE\{Company}\Update\Clients\{AppID} → pv to the installed
version of the application. If pv is present and valid in the app's Clients
key it will be used by the updater as the source of truth for the registered
version.

For backwards compatibility with third party software, on Windows, after a
successful registration and on each update, the updater will set
[HKCU or HKLM]\SOFTWARE\{Company}\Update\ClientState\{AppID} → pv to the
version of the application.

### App Activity Reporting
Applications can report whether they are actively used or not through the
updater. Update servers can then aggregate this information to produce user
counts.

#### Windows:
*   When active, the application sets
    HKCU\SOFTWARE\{Company}\Update\ClientState\{AppID} → dr (REG_SZ): 1.
    *   Note: both user-scoped and system-scoped updaters use HKCU.
*   When reporting active use, the updater resets the value to 0.

#### macOS:
*   The application touches
    ~/Library/{Company}/{Company}SoftwareUpdate/Actives/{APPID}.
*   The updater deletes the file when reporting active use.

### OEM Features

#### Background

*   There are two types of OEM installs, which differ between OEMs and
    geography:
    *   Machine EULA covering the app and the updater
        *   A machine EULA is an OEM-specific EULA displayed during the Windows
            Out Of the Box Experience (OOBE). In some cases, the OEM's EULA
            covers Google's products. App developers should consult with legal
            counsel and make sure that the EULA also includes auto-updates and
            the updater.
    *   No Machine EULA
        *   General rule: OEMs should use Windows Audit Mode to install and not
            enter OOBE.
*   Large OEMs prepare new computers in factories without network access.
*   Some OEMs prepare one computer and replicate the image on many computers.

#### Application Integration

In most cases, the Technical Account Manager (TAM) will generate a wrapper
around the installer that specifies the correct command line.

##### All OEM Cases

*   The app must support per-machine installs.
*   The app must be installed using a standalone installer.
*   The updater command line must include the "/oem" switch.
*   The app installer must not generate unique IDs during \[OEM\] install.
*   The app installer must not write to HKCU during install (this will be
    deleted when the OEM exits audit mode). This is a general guideline, but
    worth re-iterating.
*   The app installer must not ping or attempt to use the network.
*   The app installer must not launch the app. If the app uses the Installer API
    to have the updater launch the app, the updater will not launch it when run
    silently by the OEM.

###### Detecting OEM Install From App Installer

During an OEM install (/oem), the updater writes the registry value
`OemInstallTime` \[REG_DWORD\] to
`HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Google\Update\Clients`.
The value of `OemInstallTime` is the value in minutes since the Windows Epoch.
The presence of `OemInstallTime` should be considered an OEM install. The
updater deletes this value on the next wake after 72 hours.

##### Machine EULA Case

This is the best case for the app (and users) because the updater will be able
to update applications as soon as the end-user turns on the computer (post
OOBE).

*   The updater install command line must ***not*** include the "/eularequired"
    switch if the OEM EULA encompasses the updater and the app.

##### Non-Machine EULA Case

*   The updater command line must include the "/eularequired" switch.
*   Display a EULA on first run and do not allow the user to use the application
    without accepting it.
    *   Usage stats cannot be enabled when installed by the OEM, so you may wish
        to offer the user the chance to opt-in on the EULA screen.
*   When the EULA is accepted, notify the updater by writing eulaaccepted=1 in
    the app's `ClientState` or `ClientStateMedium` key.
*   See the section on `EULA/ToS Acceptance`.

#### OEM Features

The updater has the following features that support OEM installs.

*   Supports standalone installers, which can install the app without a network
    connection in the OEM factory.
*   Supports silent installs and reports errors in any child processes as the
    exit code, allowing the OEM to easily determine success (exit code 0) or
    failure.
*   Detects "OEM factory mode" and behaves correctly:
    *   Does not ping or otherwise use the network in the OEM factory.
    *   Does not create unique IDs in OEM factory.
    *   Does not exit "OEM mode" if the OEM boots the system into non-audit
        mode.
*   Does not use the network (i.e for update checks) until application's EULA is
    accepted when installed in the non-machine EULA case.

### EULA/ToS Acceptance
Most commonly, users accept relevant Terms of Service before downloading or
installing the updater.

The updater can be installed in "eula-required" mode by passing the install
process the `--eularequired` switch. While in eula-required mode, the updater
will not update software nor make any communications to the server, with the
following exceptions:
*   The updater will report its own uninstallation to the server, if the user
    takes manual action to uninstall it.
*   If the user has agreed to send usage stats / crash reports, the updater will
    transmit those. (This case may be vacuous.)

In eula-required mode, the updater will still perform offline installations and
respond as necessary to requests about its version and product set. It will not
check for device policies or domain enrollment.

If a user installs an app using an online installer, the updater will transition
out of eula-required mode and begin normal operation.

On Windows, applications can signal the updater that the user has accepted Terms
of Service by writing
`HKLM\SOFTWARE\{Company}\Update\ClientStateMedium\{AppID}` → `eulaaccepted`
(DWORD): `1`. The updater will then transition out of eula-required mode and
begin normal operation the next time it runs periodic tasks.

Once operating normally, the updater only returns to eula-required mode when
it is uninstalled and then reinstalled with `--eularequired`.

### Windows: checking if EULA has already been accepted
*   Applications can check if the EULA has already been accepted by checking
    whether the value `eulaaccepted` does not exist at
    `HKCU|HKLM\SOFTWARE\{Company}\Update`, or if it does exist, that it has a
    value of `(DWORD): 1`.

### Usage Stats Acceptance
The updater may upload its crash reports and send usage stats if and only if
any piece of software it manages is permitted to send usage stats.

#### Windows:
*   Applications enable usage stats by writing:
    `HKCU\SOFTWARE\{Company}\Update\ClientState\{APPID}` → usagestats
    (DWORD): 1 for user install,
    and either
    `HKLM\SOFTWARE\{Company}\Update\ClientStateMedium\{APPID}` → usagestats
    (DWORD): 1 or
    `HKLM\SOFTWARE\{Company}\Update\ClientState\{APPID}` → usagestats
    (DWORD): 1 for system install.
*   Applications rescind this permission by writing a value of 0.

#### macOS:
*   Application enable usage stats by setting `IsUploadEnabled` to true for a
    crashpad database maintained in a "Crashpad" subdirectory of their
    application data directory.
*   The updater searches the file system for Crashpad directories belonging
    to {Company}.

### Telemetry
When the updater installs an application (an installer is run) it sends an
event with `"eventtype": 2` indicating the outcome of installation. The updater
does not send such a ping for its own installation.

When the updater updates an application (including itself) it sends an
event with `"eventtype": 3` indicating the outcome of update operation.

When the updater detects the uninstallation of an application, it sends an
event with `"eventtype": 4` to notify the server of the uninstallation.

When the updater attempts to download a file, it sends an event with
`"eventtype": 14` describing the parameters and outcome of the download.

Multiple events associated with an update session are bundled together into a
single request.

### Downloading
There could be multiple URLs for a given application payload. The URLs are tried
in the order they are returned in the update response.

The integrity of the payload is verified.

There is no download cache. Payloads are re-downloaded for applications which
fail to install.

### Logging
All updater logs are written to `{UPDATER_DATA_DIR}\updater.log`.

After the log reaches 5 MiB in size, the updater will attempt to move it to
`{UPDATER_DATA_DIR}\updater.log.old` when starting, replacing any existing file
there. The log rotation may be delayed if another updater process is running.

On macOS for system-scope updaters, `{UPDATER_DATA_DIR}` is
`/Library/Application Support/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}`.

On macOS for user-scope updaters, `{UPDATER_DATA_DIR}` is
`~/Library/Application Support/{COMPANY_SHORTNAME}/{PRODUCT_FULLNAME}`.

On Windows for system-scope updaters, `{UPDATER_DATA_DIR}` is
`%PROGRAMFILES%\{COMPANY_SHORTNAME}\{PRODUCT_FULLNAME}`. (A 32-bit updater uses
use `%PROGRAMFILESX86%` if appropriate instead.)

On Windows for user-scope updaters, `{UPDATER_DATA_DIR}` is
`%LOCALAPPDATA%\{COMPANY_SHORTNAME}\{PRODUCT_FULLNAME}`.

On Windows, when the updater uninstalls itself, and there are no other versions
of the updater in existence for the scope, the updater saves a copy of the final
log file to `Windows\SystemTemp\updater.log` for system installs, and
`%TMP%\updater.log` for user installs.

## Network

#### Proxy detection and authentication (Windows)
The updater uses the proxy configuration defined by cloud policy or Windows
proxy settings, in this order of priority.

Windows proxy settings are defined per-system or per-user. If no user is logged
in when the updater is running, then WinHTTP per-system proxy settings are
used. Otherwise, the updater impersonates one of the logged in users, and uses
the corresponding proxy settings for that user.

The proxy settings include a combination of auto-proxy (WPAD), proxy
auto-configuration, or named proxy. The updater tries one of these mechanisms
in the order described above.

## Services

### Crash Reporting
The updater uses Crashpad for crash reporting. Each updater process spawns a
crash handler child process. Each crash handler process is capable of uploading
crashes.

### Process Launcher
The feature allows installed products to pre-register and later run elevated
command lines in the format `c:\program files\foo\exe.exe params`. Multiple
command lines can be registered per `app_id`.

This feature is only for system applications.

The program path is always an absolute path. Additionally, the program path has
to be a child of %ProgramFiles% or %ProgramFiles(x86)%. For instance:
* `c:\path-to-exe\exe.exe` is an invalid path.
* `"c:\Program Files\subdir\exe.exe"` is a valid path.
* `"c:\Program Files (x86)\subdir\exe.exe"` is also a valid path.

#### Registration
Registration is the same as for App commands, except there are no replaceable
parameters. See
[App command registration](functional_spec.md#services-application-commands-applicable-to-the-windows-version-of-the-updater-registration).

#### Usage
Once registered, commands may be invoked using the `LaunchCmdElevated` method in
the `IProcessLauncher` interface.

### Application Commands (applicable to the Windows version of the Updater)
The feature allows installed products to pre-register and later run command
lines in the format `c:\path-to-exe\exe.exe {params}` (elevated for system
applications). `{params}` is optional and can also include replaceable
parameters substituted at runtime. Multiple app commands can be registered per
`app_id`.

The program path is always an absolute path. Additionally, for system
applications,  the program path has to be a child of %ProgramFiles% or
%ProgramFiles(x86)%. For instance:
* `c:\path-to-exe\exe.exe` is an invalid path.
* `"c:\Program Files\subdir\exe.exe"` is a valid path.
* `"c:\Program Files (x86)\subdir\exe.exe"` is also a valid path.

#### Registration
App commands are registered in the registry with the following format:

```
    Update\Clients\{`app_id`}\Commands\`command_id`
        REG_SZ "CommandLine" == {command format}
        {optional} REG_DWORD "AutoRunOnOSUpgrade" == {1}
```

* There is a deprecated command layout format that is only supported for
versions of Google Chrome `110.0.5435.0` and below with the `cmd` command id.
```
    Update\Clients\{`app_id`}
        REG_SZ `command_id` == {command format}
```

Example `{command format}`: `c:\path-to\echo.exe %1 %2 %3 StaticParam4`

As shown above, `{command format}` needs to be the complete path to an
executable followed by optional parameters.

If "AutoRunOnOSUpgrade" is non-zero, the command is invoked when the updater
detects an OS upgrade. In this case, `command format` can optionally contain a
single substitutible parameter, which is filled in with the OS versions in the
format `{Previous OS Version}-{Current OS Version}`. It is ok to have a static
command line as well if the OS versions information is not required.

#### Usage
Once registered, commands may be invoked using the `execute` method in the
`IAppCommandWeb` interface.

```
interface IAppCommandWeb : IDispatch {
  // Use values from the AppCommandStatus enum.
  [propget] HRESULT status([out, retval] UINT*);
  [propget] HRESULT exitCode([out, retval] DWORD*);
  [propget] HRESULT output([out, retval] BSTR*);
  HRESULT execute([in, optional] VARIANT substitution1,
                  [in, optional] VARIANT substitution2,
                  [in, optional] VARIANT substitution3,
                  [in, optional] VARIANT substitution4,
                  [in, optional] VARIANT substitution5,
                  [in, optional] VARIANT substitution6,
                  [in, optional] VARIANT substitution7,
                  [in, optional] VARIANT substitution8,
                  [in, optional] VARIANT substitution9);
};
```

Here is a code snippet a client may use, with error checking omitted:

```
var bundle = update3WebServer.createAppBundleWeb();
bundle.initialize();
bundle.createInstalledApp(appGuid);
var app = bundle.appWeb(0);
cmd = app.command(command);
cmd.execute();
```

Parameters placeholders (`%1-%9`) are filled by the numbered substitutions in
`IAppCommandWeb::execute`. Placeholders without corresponding substitutions
cause the execution to fail.

Clients may poll for the execution status of commands that they have invoked by
using the `status` method of `IAppCommandWeb`. When the status is
`COMMAND_STATUS_COMPLETE`, the `exitCode` method can be used to get the process
exit code.

#### Command-Line Format
* for system applications, the executable path has to be a child of
`%ProgramFiles%` or `%ProgramFiles(x86)%` for security, since it runs elevated.
* placeholders are not permitted in the executable path.
* placeholders take the form of a percent character `%` followed by a digit.
Literal `%` characters are escaped by doubling them.

For example, if substitutions to `IAppCommandWeb::execute` are `AA` and `BB`
respectively, a command format of:
  `echo.exe %1 %%2 %%%2`
becomes the command line
  `echo.exe AA %2 %BB`

#### Signature verification
Signature verification is not done on the app command executables, since the
AppCommands will always be running from a secure location for system, and the
key that defines the application command path is in HKLM, both of which mitigate
the threat of a non-admin attacker. An Admin attacker would already be able to
bypass any signature checking by binplanting a DLL, or just by performing
whatever changes they like on the system, so is outside the threat model.

#### Telemetry
A ping with the value `kEventAppCommandComplete` = `41` is sent if usagestats
are enabled after an app command completes execution.

The app command id is reported in the `appcommandid` attribute in the ping. The
`appcommandid` attribute is used along with the `appid` to uniquely identify the
app command associated with the ping.

If the app command launched successfully, the result returned by the app command
process will be reported in `error` in the ping.

If the app command fails to launch, code `kErrorAppCommandLaunchFailed ` is
reported in the `extra_code1` in the ping, along with the actual error code that
caused that launch failure in `error`.

If the app command times out, code `kErrorAppCommandTimedOut` is reported in the
`extra_code1` in the ping, along with the error code
`HRESULT_FROM_WIN32(ERROR_TIMEOUT)` in `error`.

### Policy Status API
The feature allows Chrome and other applications to query the policies that are
currently in effect.

Chrome Browser Enterprise (CBE) admins sometimes want to understand if the
update policies they have set have propagated to the clients.

Without this API, the only way they can do this is to open up regedit to see if
the GPO has propagated correctly.

In addition there is a delay between when the GPO is set on the server and when
the value is propagated on the client so being able to verify that the updater
picks up the policy can help debug propagation issues as well.

The IPolicyStatus/IPolicyStatus2/IPolicyStatus3/IPolicyStatus4 interfaces
therefore expose this functionality that can be queried and shown in
chrome://policy.

[IPolicyStatus/IPolicyStatus2/IPolicyStatus3/IPolicyStatus4 interface definition]
(https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/app/server/win/updater_legacy_idl.template?q=IPolicyStatus)

## Uninstallation
On Mac and Linux, if the application was registered with an existence path
checker and no file at that path exists, the updater considers the application
uninstalled, sends the ping, and stops trying to keep it up to date. User-scope
updaters will also do this if the file is owned by the root user.

On Windows, if the Clients entry for for the application is deleted, the app is
considered uninstalled.

On Windows, the updater registers a "UninstallCmdLine" under the `Software\
{Company}\Updater` key. This command line can be invoked by application
uninstallers to cause the updater to update its registrations. The updater
also checks for uninstallations in every periodic task execution.

When the last registered application is uninstalled, the updater uninstalls
itself immediately. The updater also uninstalls itself if it has started
24 times but never had a product (besides itself) registered for updates.

The updater uninstaller removes all updater files, registry keys, RPC hooks,
scheduled tasks, and so forth from the system, except that:
*   it leaves a small log file in its data directory.
*   it leaves the Clients registry key in Windows registry.

Inactive instances of the updater uninstall themselves (but not the updater
overall) once the active version of the updater is higher than the inactive
instance's version. Additionally, as part of its periodic tasks, the active
updater will trigger the uninstallation of old instances of the updater and
clean up any files they leak.

## Associated Tools

### External Constants Overrides
Building the updater produces both a production-ready updater executable and a
version of the executable used for the purposes of testing. The test executable
is identical to the production one except that it allows certain constants to be
overridden by the execution environment:

*   `url`: List of URLs for update check & ping-back.
*   `crash_upload_url`: Crash reporting URL.
*   `device_management_url`: URL to fetch device management policies.
*   `initial_delay`: Time to delay the start of the automated background tasks.
*   `overinstall_timeout`: Over-install timeout.
*   `server_keep_alive`: Minimum amount of time the server needs to stay alive.
*   `use_cup`: Whether CUP is used at all.
*   `cup_public_key`: An unarmored PEM-encoded ASN.1 SubjectPublicKeyInfo with
    the ecPublicKey algorithm and containing a named elliptic curve.
*   `group_policies`: Allows setting group policies, such as install and update
    policies.
*   `crx_verifier_format`: An integer value to guide how to verify the CRX file.
       - 0: CRX3.
       - 1: CRX3 with test publisher proof.
       - 2: CRX3 with production publisher proof.
*   `idle_check_period`: The idleness check period.
*   `managed_device`: Whether the device is enterprise managed.

Overrides are specified in an overrides.json file placed in the updater data
directory.

### Tagging Tools
The project contains a helper tool for tagging called `tag.exe`.
This tool can be
[used](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag_main.cc)
to inject a superfluous certificate into a signed binary to support the
creation of tagged binaries.
