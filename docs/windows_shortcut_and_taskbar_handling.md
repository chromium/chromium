# Windows Shortcut and Pinned Taskbar Icon handling

When Chrome is installed on Windows, it creates a shortcut on the desktop that
launches Chrome. It also adds the same shortcut to the start menu. These
shortcuts do not specify a profile, so they launch Chrome with the most recently
used profile.

Windows allows users to pin applications to the taskbar. When a user
pins an application to the taskbar, Windows looks for a desktop shortcut that
matches the application, and if it finds one, it creates a .lnk file in the
directory
`<user dir>\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar.`
If it does not find a matching desktop shortcut, it creates an 8-hex-digit
sub-directory of
`<user dir>\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\ImplicitAppShortcuts\`
and puts the .lnk file in that directory. For example, 3ffff1b1b170b31e.

App windows on Windows have an
[App User Model ID (AUMI)](https://docs.microsoft.com/en-us/windows/win32/shell/appids)
property. For Chrome windows, this is set in
[BrowserWindowPropertyManager::UpdateWindowProperties](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/frame/browser_window_property_manager_win.cc?q=BrowserWindowPropertyManager::UpdateWindowProperties),
when a window is opened. Windows desktop shortcuts have an app model property,
and this should match the open window's AUMI. Windows groups open windows with
the same AUMI to a taskbar icon.

There are two kinds of Chrome windows with AUMI's: browser windows, and app
windows, which include web apps, and extensions, i.e., windows opened via
--app-id or --app.

[GetAppUserModelIdForBrowser](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/shell_integration_win.cc?q=GetAppUserModelIdForBrowser)
constructs an AUMI for a browser window and
[GetAppUserModelIdForApp](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/shell_integration_win.cc?q=GetAppUserModelIdForApp)
constructs an AUMI for an app window. Each calls
[ShellUtil::BuildAppUserModelId](https://source.chromium.org/chromium/chromium/src/+/main:chrome/installer/util/shell_util.cc;q=ShellUtil::BuildAppUserModelId)
to construct the AUMI out of component strings.

All AUMI's start with the base app id,
[install_static::GetBaseAppId](https://source.chromium.org/chromium/chromium/src/+/main:chrome/install_static/install_util.cc?q=install_static::GetBaseAppId).
This varies for different Chrome channels (e.g., Canary vs. Stable) and
different Chromium-based browsers (e.g., Chrome vs. Chromium).

The AUMI for a browser app has the format:
`<BaseAppId>.<app_name>[.<profile_name>]`.
profile_name is only appended when it's not the default profile.

The AUMI for a Chrome browser window has the format:
`<BaseAppId>[browser_suffix][.profile_name]`.
profile_name is only appended when it's not the default profile.
browser_suffix is only appended to the BaseAppId if the installer
has set the kRegisterChromeBrowserSuffix command line switch, e.g.,
on user-level installs.

Since AUMI's for browser and app windows include the profile_name, each
profile's windows will be grouped together on the taskbar.

shell_integration_win.cc has a function [GetExpectedAppId](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/shell_integration_win.cc?q=GetExpectedAppid)
to determine what the AUMI for a shortcut should be. It also has a function
[MigrateTaskbarPins](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/shell_integration_win.cc?q=MigrateTaskbarPins)
to migrate pinned taskbar icons if the AUMI's need to change.

## Multi-profile Support
When the user has more than one profile, the shortcuts are renamed to include
the profile name, e.g., `Chrome.lnk` becomes `<profile name> - Chrome`.  The
shortcut icons, both desktop and taskbar, are badged with their profile icon.
This badged icon is also used in the tab preview for a Chrome window.

## Diagnosing Issues
To dump a taskbar icon's properties, run this command:

```
vpython3 \src\chromium\src\chrome\installer\tools\shortcut_properties.py \
    --dump-all \
    "%APPDATA%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar"
```

This shows you the properties of all the taskbar pinned icons. If the taskbar
icon is in a subdirectory of ImplicitApps, pass that directory to
shortcut_properties.py.
