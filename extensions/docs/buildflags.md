As of August 2025, the extensions team is working to add support for an
experimental desktop Android build configuration. Because there's too much
extensions code to convert at once, the team is doing it gradually, via build
flags and their corresponding GN arguments.

`ENABLE_EXTENSIONS` - Historically all extensions code was guarded by this build
flag. There are too many uses in the code base to change its meaning all at
once. What it means now is extensions for Win/Mac/Linux/ChromeOS but *not*
desktop Android. When used in an //extensions directory it means extensions code
that runs on Win/Mac/Linux/ChromeOS and the team intends to port it to desktop
Android.

`ENABLE_EXTENSIONS_CORE` - All new code should use this. It means extensions
code for all platforms: Win/Mac/Linux/ChromeOS/desktop Android. You will need to
include the header `extensions/buildflags/buildflag.h` to use this flag.

`ENABLE_DESKTOP_ANDROID_EXTENSIONS` - Rarely used. Indicates a piece of
extensions code that only runs on the desktop Android build.

`IS_ANDROID` - Rarely used. Most often indicates code that has fundamental
platform differences, such as for code that has (non-abstracted) UI dependencies
or other low-level platform changes. In the long run, this should be used in the
same types of situations you'd see IS_WIN or other platforms. Also occasionally
used to exclude code that is only supported under manifest V2, because desktop
Android only supports manifest V3.

`ENABLE_PLATFORM_APPS` - Rarely used. Typically used to exclude Chrome Platform
Apps code from the rest of the extensions system. Platform Apps were app-like
pieces of software built with JS/HTML/CSS and resembling Chrome extensions. The
Chrome apps platform has been deprecated on Windows, Mac and Linux since ~2018.
Chrome OS is in the process of deprecating it, with managed users reaching end
of life in 2028.

The plan in the long run is to convert almost everything to
`ENABLE_EXTENSIONS_CORE`, then rename that flag back to `ENABLE_EXTENSIONS`.
