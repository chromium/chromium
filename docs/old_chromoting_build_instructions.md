# Chromoting Build Instructions

Chromoting, also known as
[Chrome Remote Desktop](https://support.google.com/chrome/answer/1649523),
allows one to remotely control a distant machine, all from within the Chromium
browser. Its source code is located in the `remoting/` folder in the Chromium
codebase. For the sake of brevity, we'll assume that you already have a
pre-built copy of Chromium (or Chrome) installed on your development computer.

[TOC]

## Obtain API keys

Before you can compile the code, you must obtain an API key to allow it to
access the federated Chrome Remote Desktop API.

1.  Join the chromium-dev list, which can be found at
    https://groups.google.com/a/chromium.org/forum/#!forum/chromium-dev. (This
    step is required in order to gain access to the Chromoting API.)
1.  Visit the Google APIs console at https://code.google.com/apis/console.
    1.  Use the `API Project` dropdown to create a new project with a name of
        your choice.
    1.  Click on `APIs & Auth > APIs`.
    1.  Search for `Chrome Remote Desktop API`.
    1.  Click on the `Chrome Remote Desktop API` search result.
    1.  Click on `Enable API`.
    1.  Click on `APIs & Auth > Credentials`.
    1.  Click on `Add Credentials`.
    1.  Choose `OAuth 2.0 client ID`.
    1.  Choose `Chrome App`.
    1.  Under application id, enter `ljacajndfccfgnfohlgkdphmbnpkjflk`.

## Obtain Chromium code

If you've already checked out a copy of the browser's codebase, you can skip
this section, although you'll still need to run `gclient runhooks` to ensure you
build using the API keys you just generated.

1.  [Install the build dependencies](linux_build_instructions_prerequisites.md).
1.  Install the depot\_tools utilities, a process that is documented at
    https://dev.chromium.org/developers/how-tos/install-depot-tools.
1.  Download the Chromium source code by running:
    `$ fetch chromium --nosvn=True`

## Build and install the Linux host service

If you want to remote into a (Debian-based) GNU/Linux host, follow these steps
to compile and install the host service on that system. As of the time of
writing, you must compile from source because no official binary package is
being distributed.

1.  Start in the `src/` directory that contains your checkout of the Chromium
    code.
1.  Build the Chromoting host binaries:

    ```shell
    $ autoninja -C out/Release remoting_me2me_host remoting_start_host \
    remoting_native_messaging_host remoting_native_messaging_manifests
    ```

    (`autoninja` is a wrapper that automatically provides optimal values for the
    arguments passed to `ninja`.)

1.  When the build finishes, move into the installer directory:
    `$ cd remoting/host/installer/`
1.  Generate a DEB package for your system's package manager:
    `$ linux/build-deb.sh`
1.  Install the package on your system: `$ sudo dpkg -i *.deb`
1.  The next time you use the Chromoting extension from your browser, it should
    detect the presence of the host service and offer you the option to
    `Enable remote connections`.
    1.  If the Web app doesn't properly detect the host process, you may need to
        create a symlink to help the plugin find the native messaging host:
        `$ sudo ln -s /etc/opt/chrome /etc/chromium`

(NB: If you compile the host service from source and expect to configure it
using the browser extension, you must also compile the latter from source.
Otherwise, the package signing keys will not match and the Web app's OAuth2
token will not be valid within the host process.)

## Build and install the Chrome packaged app

The Web app is the Chromoting system's main user interface, and allows you to
connect to existing hosts as well as set up the host process on the machine
you're currently sitting at.  Once built, it must be installed into your browser
as an extension.

1.  Start in the `src/` directory that contains your checkout of the Chromium
    code.
1.  Build the browser extension (Be sure to replace the substitutions denoted by
    angled braces.):

    ```shell
    $ GOOGLE_CLIENT_ID_REMOTING_IDENTITY_API=<client id> \
    autoninja -C out/Release remoting_webapp
    ```

1.  Install the extension into your Chromium (or Chrome) browser:
    1.  Visit the settings page [chrome://extensions].
    1.  If it is unchecked, tick the `Developer mode` box.
    1.  Click `Load unpacked extension...`, then navigate to
        `out/Release/remoting/remoting.webapp.v2/` within your code checkout.
    1.  Confirm the installation, open a new tab, and click the new app's
        Chromoting icon.
    1.  Complete the account authorization step, signing into your Google
        account if you weren't already.

## Build and install the Android client

If you want to use your Android device to connect to your Chromoting hosts,
follow these steps to install the client app on it. Note that this is in the
very early stages of development. At the time of writing, you must compile from
source because no official version is being distributed.

1.  Follow all the instructions under the `Getting the code` and
    `Install prerequisites` sections of:
    https://www.chromium.org/developers/how-tos/android-build-instructions
1.  Move into the `src/` directory that contains your checkout of the Chromium
    code.
1.  Build the Android app: `$ autoninja -C out/Release remoting_apk`
1.  Connect your device and set up USB debugging:
    1.  Plug your device in via USB.
    1.  Open the Settings app and look for the `Developer options` choice.
        1.  If there is no such entry, open `About phone`, tap `Build number`
            7 times, and look again.
    1.  Under `Developer options`, toggle the main switch to `ON` and enable
        `USB debugging`.
1.  On your machine and still in the `src/` directory, run:
    `$ build/android/adb_install_apk.py --apk=out/Release/apks/Chromoting.apk`
1.  If your Android device prompts you to accept the host's key, do so.
1.  The app should now be listed as Chromoting in your app drawer.

See the [chromoting_android_hacking.md] guide for instructions on viewing the
Android app's log and attaching a debugger.
