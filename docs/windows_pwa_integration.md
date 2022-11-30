# Windows Progressive Web App integration

## Desktop Shortcuts
When a Progressive Web App (PWA) is installed on Windows, Chrome creates a
desktop shortcut to the PWA, with the PWA icon. The shortcut launches a small
Chrome binary chrome_proxy.exe with the app id of the PWA, and the Chrome
profile the PWA is installed in. When chrome_proxy.exe runs, it launches Chrome
with the same command line options. The shortcut links to chrome_proxy.exe
instead of chrome.exe because of
 [a bug in Windows 10 start menu pinning](https://source.chromium.org/chromium/chromium/src/+/main:chrome/chrome_proxy/chrome_proxy_main_win.cc;l=23).

## File handling support
In order to make Progressive Web Apps (PWA's) more like traditional apps, PWA's
support opening files on the user's desktop. On Windows, when a PWA is
installed, if the PWA's manifest lists one or more file extension types that it
supports opening, Chrome registers the PWA as a handler for the file
extension(s), in the Windows registry. When the user right clicks on a file with
a registered extension, the PWA name and custom icon appears in the list of
applications that can open the file. The user can also set the PWA as the
default handler for the file extension.

Because of a limitation of the Windows shell, Chrome registers a per-PWA install 
[launcher app](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/chrome_pwa_launcher/README.md;l=1) 
 as a handler for the file extension. Chrome ships with a canonical launcher app
 called chrome_pwa_launcher.exe, which lives in the version sub-directory of the
 Chrome install dir. When a PWA is installed, we create a hard link from the
 PWA install dir `<profile_dir>/Web Applications/<app_id>` to the canonical launcher
 app. If the hard link fails (e.g., Chrome install dir is on a different drive
 than the profile dir), we copy the launcher app to the PWA install dir. In either
 case, the name of the launcher app in the PWA install dir is a sanitized version
 of the PWA name.

Registration starts in [web_app::RegisterFileHandlersWithOS](https://source.chromium.org/search?q=RegisterFileHandlersWithOS%20file:_win.cc&sq=),
 and works as follows: we create a unique
 [ProgID](https://docs.microsoft.com/en-us/windows/win32/com/-progid--key)
 for the PWA installation with the following format:
`<BaseAppId>.<hash(Profile + AppID)>`
We use the hash due to 32-character limit for ProgID's. The registry work is
done in
[ShellUtil::AddFileAssociations](https://source.chromium.org/chromium/chromium/src/+/main:chrome/installer/util/shell_util.cc?q=%20ShellUtil::AddFileAssociations):

* Register the ProgID by adding key `HKCU\Software\Classes\<progID>` to the registry.
* Set the application name and icon for the PWA with these two keys:
    * `HKCU\Software\Classes\<progID>\Application::ApplicationIcon = <path to icon in PWA install dir>,0`
    * `HKCU\Software\Classes\<progID>\Application::ApplicationName = <PWA name>`
* Hook up the command to launch the launcher app
    * `HKCU\Software\Classes\<progID>\shell\open\command = <launcher_app_path_in_profile> --app-id=<app_id> --profile-directory=<profile_dir>`
* Add a key to keep track of the file extensions registered for a progId,
for ease of uninstallation:
    * `HKCU\Software\Classes\<progID>\File Extensions = <semicolon delimited list of extensions>`

When Chrome is launched, it writes its path into the "Last Browser" file in
the User Data dir.
When the launcher app is run, it launches Chrome using the path written into the
"Last Browser" file. Because the launcher app is in a sub-directory of the profile
directory, the "Last Browser" file is in its great grandparent directory.

When a new version of Chrome is installed, we need to update the hard links
to and copies of the installed launcher apps to use the newly installed canonical
launcher app. This is done by having the launcher app pass its version to Chrome, when
launching Chrome. If the launcher app is out of date, Chrome updates all the
launcher apps in the current user data dir.

When a PWA is uninstalled, we unregister the PWA as a handler for the file
extensions it was registered for. When a PWA changes the file extensions it can
handle, we update the registry.

## Miscellaneous
 * If the same PWA is registered in multiple profiles, we distinguish them by
adding the profile name in parentheses to the PWA name, e.g,
"Example PWA (profile1)". If a PWA is uninstalled from a  profile, and there is
one remaining install in another profile, we remove the profile name from the
application name.
 * Windows 7 does not support some of the registry entries needed to set the
 name and icon for a PWA. So, the file open context menu item for a PWA on
 Windows 7 gets its name from the launcher app created for the PWA, and uses a
 generic PWA icon.
