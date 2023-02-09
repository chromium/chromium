# Chromium Updater Migration Test Instructions

This document describes how to do basic developer testing with [Chromium
Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/)
on systems or with software that already uses Keystone or Omaha 3. The intended
audience is Google developers who work on software that uses Keystone (or Omaha
3) and want to verify that their software works as expected with Chromium
Updater.

## Step 1: Install Keystone or Omaha 3
Install your product as you normally would, and run it. This should result in
a successful setup of the updater. It is useful to install an old version of
your product so that you can test that it updates correctly.

The updater can be installed at either per-user or system-wide scopes; the
instructions will vary depending on which type of installation your product
uses.

## Step 2: Opt-In to Chromium Updater Deployment
On macOS, and with a per-user Keystone (this is common), run:
```
~/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Helpers/ksadmin -C chromium-updater-opt-in -P com.google.Keystone && ~/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Helpers/ksadmin --install -P com.google.Keystone && sleep 10 && ~/Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --wake --enable-logging --vmodule="*/updater/*=2"
```

On macOS, and with a system-wide Keystone, run:
```
sudo /Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Helpers/ksadmin -C chromium-updater-opt-in -P com.google.Keystone -S && /Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Resources/CheckForUpdatesNow.command && sleep 10 && sudo /Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --wake --system --enable-logging --vmodule="*/updater/*=2"
```

On Windows, and with a per-user Omaha 3, use `regedit.exe` to open the key
`HKEY_CURRENT_USER\SOFTWARE\Google\Update\ClientState\{430FD4D0-B729-4F61-AA34-91526481799D}\cohort`
and set the value of `hint` (`REG_SZ`) to `chromium-updater-opt-in`. Then,
delete the `LastChecked` value from `HKEY_CURRENT_USER\SOFTWARE\Google\Update`.
Then, run `taskschd.msc`, click `Task Scheduler Library`, and run a
`GoogleUpdateTaskUserUA` task. Wait 5 minutes and then run
`%LOCALAPPDATA%\Google\GoogleUpdater\updater.exe --wake --enable-logging --vmodule="*/updater/*=2"`
from an the command prompt (non-admin).

On Windows, and with a system-wide Omaha 3, use `regedit.exe` to open the key
`HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Google\Update\ClientState\{430FD4D0-B729-4F61-AA34-91526481799D}\cohort`
and set the value of `hint` (`REG_SZ`) to `chromium-updater-opt-in`. Then,
delete the `LastChecked` value from `HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Google\Update`.
Then, run `taskschd.msc`, click `Task Scheduler Library`, and run a
`GoogleUpdateTaskMachineUA` task. Wait 5 minutes and then run
`%PROGRAMFILES(X86)%\Google\GoogleUpdater\updater.exe --wake --system --enable-logging --vmodule="*/updater/*=2"`
from an admin command prompt.

## Step 3: Check that Chromium Updater is Installed
On macOS, check that
`~/Library/Application Support/Google/GoogleUpdater` (per-user) or
`/Library/Application Support/Google/GoogleUpdater` (per-system) exists.

On Windows, check that `%LOCALAPPDATA%\Google\GoogleUpdater` (per-user) or
`%PROGRAMFILES(X86)%\Google\GoogleUpdater` (system-wide) exists.

## Step 4: Test Your Software
If your software uses any updater APIs (such as checking for updates on-demand,
or Windows app commands), exercise those APIs and verify that they work as
expected.

If you installed an old version of your software, verify that Chromium Updater
can update your software. You can either wait (up to 6 hours) for Chromium
Updater to update it automatically, or you can trigger an early check by
running:

(macOS, per-user):
`~/Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --wake --enable-logging --vmodule="*/updater/*=2"`

(macOS, system-wide):
`sudo /Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --wake --system --enable-logging --vmodule="*/updater/*=2"`

(Windows, per-user):
`%LOCALAPPDATA%\Google\GoogleUpdater\*\updater.exe --wake --enable-logging --vmodule="*/updater/*=2"`

(Windows, system-wide) in command prompt running as admin:
`%PROGRAMFILES(X86)%\Google\GoogleUpdater\*\updater.exe --wake --system --enable-logging --vmodule="*/updater/*=2"`

## Step 5: Removing Chromium Updater
To remove Chromium Updater from your system, run:

On macOS, and with a per-user updater, run:
`~/Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --uninstall && rm -rf ~/Library/Google/GoogleSoftwareUpdate`

On macOS, and with a system-wide updater, run:
`sudo /Library/Application\ Support/Google/GoogleUpdater/*/GoogleUpdater.app/Contents/MacOS/GoogleUpdater --uninstall --system && sudo rm -rf /Library/Google/GoogleSoftwareUpdate`

On Windows, and with a per-user updater, run `%LOCALAPPDATA%\Google\GoogleUpdater\*\updater.exe --uninstall`, then use `regedit.exe` to open the key
`HKEY_CURRENT_USER\SOFTWARE\Google\Update\ClientState\{430FD4D0-B729-4F61-AA34-91526481799D}\cohort`
and set the value of `hint` (`REG_SZ`) to `` (empty string).

On Windows, and with a system-wide updater, from a command prompt running as admin, run `%PROGRAMFILES(X86)%\Google\GoogleUpdater\*\updater.exe --uninstall --system`, then use `regedit.exe` to open the key
`HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Google\Update\ClientState\{430FD4D0-B729-4F61-AA34-91526481799D}\cohort`
and set the value of `hint` (`REG_SZ`) to `` (empty string).

Then, remove or reinstall any software that uses Keystone or Omaha 3 to update.

## Filing Issues
Any discovered bugs can be filed under the `Internals>Updater` component at
crbug.com. It is helpful to include the updater log files in the report. The
updater log files are at:

(macOS, per-user):
`~/Library/Application Support/Google/GoogleUpdater/updater.log`

(macOS, system-wide):
`/Library/Application Support/Google/GoogleUpdater/updater.log`

(Windows, per-user):
`%LOCALAPPDATA%\Google\GoogleUpdater\updater.log`

(Windows, system-wide):
`%PROGRAMFILES(X86)%\Google\GoogleUpdater\updater.log`

You may need to use admin permissions or `sudo` to view logs from a system-wide
updater.
