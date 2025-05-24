# User Data Directory

[TOC]

## Introduction

The user data directory contains profile data such as history, bookmarks, and
cookies, as well as other per-installation local state.

Each [profile](https://support.google.com/chrome/answer/2364824) is a
subdirectory (often `Default`) within the user data directory.

## Current Location

To determine the user data directory for a running Chrome instance:

1. Navigate to `chrome://version`
2. Look for the `Profile Path` field.  This gives the path to the profile
   directory.
3. The user data directory is the parent of the profile directory.

Example (Windows):

* [Profile Path] `C:\Users\Alice\AppData\Local\Google\Chrome\User Data\Default`
* [User Data Dir] `C:\Users\Alice\AppData\Local\Google\Chrome\User Data`

## Default Location

The default location of the user data directory is computed by
[`chrome::GetDefaultUserDataDirectory`](https://cs.chromium.org/chromium/src/chrome/common/chrome_paths_internal.h?q=GetDefaultUserDataDirectory).

Generally it varies by

* OS platform,
* branding ([Chrome vs. Chromium](chromium_browser_vs_google_chrome.md), based
  on `is_chrome_branded` in [GN
  args](https://www.chromium.org/developers/gn-build-configuration)), and
* [release channel](https://www.chromium.org/getting-involved/dev-channel)
  (stable / beta / dev / canary).

### Windows

The default location is in the local app data folder:

* [Chrome] `%LOCALAPPDATA%\Google\Chrome\User Data`
* [Chrome Beta] `%LOCALAPPDATA%\Google\Chrome Beta\User Data`
* [Chrome Canary] `%LOCALAPPDATA%\Google\Chrome SxS\User Data`
* [Chrome for Testing] `%LOCALAPPDATA%\Google\Chrome for Testing\User Data`
* [Chromium] `%LOCALAPPDATA%\Chromium\User Data`

(The canary channel suffix is determined using
[`InstallConstants::install_suffix`](https://cs.chromium.org/chromium/src/chrome/install_static/install_constants.h?q=install_suffix).)

### Mac OS X

The default location is in the `Application Support` folder:

* [Chrome] `~/Library/Application Support/Google/Chrome`
* [Chrome Beta] `~/Library/Application Support/Google/Chrome Beta`
* [Chrome Canary] `~/Library/Application Support/Google/Chrome Canary`
* [Chrome for Testing] `~/Library/Application Support/Google/Chrome for Testing`
* [Chromium] `~/Library/Application Support/Chromium`

(The canary channel suffix is determined using the `CrProductDirName` key in the
browser app's `Info.plist`.)

### Linux

The default location is in `~/.config`:

* [Chrome Stable] `~/.config/google-chrome`
* [Chrome Beta] `~/.config/google-chrome-beta`
* [Chrome Dev] `~/.config/google-chrome-unstable`
* [Chrome for Testing] `~/.config/google-chrome-for-testing`
* [Chromium] `~/.config/chromium`

(The beta and dev channel suffixes are determined from `$CHROME_VERSION_EXTRA`,
which is passed by the [launch wrapper script](https://cs.chromium.org/chromium/src/chrome/installer/linux/common/wrapper?q=CHROME_VERSION_EXTRA).)

The `~/.config` portion of the default location can be overridden by
`$CHROME_CONFIG_HOME` (since M61) or by `$XDG_CONFIG_HOME`.

Note that `$XDG_CONFIG_HOME` affects all applications conforming to the
[XDG Base Directory Spec](https://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html),
while `$CHROME_CONFIG_HOME` is specific to Chrome and Chromium.

### Chrome OS

The default location is: `/home/chronos`

### Android

The default location comes from
[Context.getDir](https://developer.android.com/reference/android/content/Context.html#getDir%28java.lang.String,%20int%29)
and is specific to the app.

Example: `/data/user/0/com.android.chrome/app_chrome`

### iOS

The default location is inside the application support directory in the app
sandbox.

* [Chrome] `Library/Application Support/Google/Chrome`
* [Chromium] `Library/Application Support/Chromium`

## Overriding the User Data Directory

### Command Line

On most platforms, the user data directory can be overridden by passing the
`--user-data-dir` command-line flag to the Chrome binary.

The override happens in `chrome/app/chrome_main_delegate.cc`. Platforms not
building with the file may not have implemented the override. Overriding the
user data directory via the command line is not supported on iOS.

Example:

* [Windows] `chrome.exe --user-data-dir=c:\foo`
* [Linux] `google-chrome --user-data-dir=/path/to/foo`

### Environment (Linux)

On Linux, the user data directory can also be overridden with the
`$CHROME_USER_DATA_DIR` environment variable.

The `--user-data-dir` flag takes precedence if both are present.

### Chrome Remote Desktop sessions (Linux)

[Chrome Remote
Desktop](https://support.google.com/chrome/answer/1649523) (CRD) used to set
`$CHROME_USER_DATA_DIR` or `$CHROME_CONFIG_HOME` on the virtual session on a
Linux host, since a single Chrome instance cannot show windows on multiple X
displays, and two running Chrome instances cannot share the same user data
directory. However, with the obsolescence of `dbus-x11`, most modern Linux
distros have lost the ability to simultaneously run multiple graphical sessions
for the same user without running into difficult-to-trace dbus cross talk
issues, and Chrome can only be run on a single X display per user in reality.
Therefore, CRD no longer sets these environment variables for new installations
after CRD host M105.

The CRD host will continue to set these environment variables if either
`chrome-config/` or `chrome-profile/` exists in
`~/.config/chrome-remote-desktop/`. If you want to use the local Chrome profile
in CRD sessions, quit Chrome and delete these folders from
`~/.config/chrome-remote-desktop/`, then reboot the host device.

### Writing an AppleScript wrapper (Mac OS X)

On Mac OS X, you can create an application that runs Chrome with a custom
`--user-data-dir`:

1. Open Applications > Utilities > Script Editor.

2. Enter:

```
set chrome to "\"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome\""
set userdatadir to "\"$HOME/Library/Application Support/Google/Chrome Alt\""
do shell script chrome & " --user-data-dir=" & userdatadir & " > /dev/null 2>&1 &"
```

3. Modify as needed for your installation path, Chrome versus Chromium, and
   desired user data directory.

4. Save the script in your Applications directory with the file format
   "Application".

5. Close the Script Editor, find your newly created application, and run it.
   This opens a Chrome instance pointing to your new profile.

If you want, you can give this application the same icon as Chrome:

1. Select the Google Chrome application and choose File > Get Info.
2. Select the icon at the top left of the info dialog.  You will see a blue
   highlight around the icon.
3. Press &#8984;C to copy the icon.
4. Open the info dialog for the new application and select the icon in the
   top left.
5. Press &#8984;V to paste the copied icon.

## User Cache Directory

On Windows and ChromeOS, the user cache dir is the same as the profile dir.
(The profile dir is inside the user data dir.)

On Mac OS X and iOS, the user cache dir is derived from the profile dir as
follows:

1. If `Library/Application Support` is an ancestor of the profile dir, the user
   cache dir is `Library/Caches` plus the relative path from `Application
   Support` to the profile dir.
2. Otherwise, the user cache dir is the same as the profile dir.

Example (Mac OS X):

* [user data dir] `~/Library/Application Support/Google/Chrome`
* [profile dir] `~/Library/Application Support/Google/Chrome/Default`
* [user cache dir] `~/Library/Caches/Google/Chrome/Default`

On Linux, the user cache dir is derived from the profile dir as follows:

1. Determine the system config dir.  This is `~/.config`, unless overridden by
   `$XDG_CONFIG_HOME`.  (This step ignores `$CHROME_CONFIG_HOME`.)
2. Determine the system cache dir.  This is `~/.cache`, unless overridden by
   `$XDG_CACHE_HOME`.
3. If the system config dir is an ancestor of the profile dir, the user cache
   dir is the system cache dir plus the relative path from the system config
   dir to the profile dir.
4. Otherwise, the user cache dir is the same as the profile dir.

Example (Linux):

* [user data dir] `~/.config/google-chrome`
* [profile dir] `~/.config/google-chrome/Default`
* [user cache dir] `~/.cache/google-chrome/Default`

On Android, the user cache directory comes from
[Context.getCacheDir](https://developer.android.com/reference/android/content/Context.html#getCacheDir%28%29).
