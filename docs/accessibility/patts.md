# The Chrome OS PATTS speech synthesis engine

Chrome OS comes with a speech synthesis engine developed internally at Google
called PATTS. It's based on the same engine that ships with all Android devices.

[Read more about Text-to-Speech in Chrome](tts.md).

[See also the eSpeak engine](espeak.md).

## Building from source

This is for Googlers only.

Visit [http://go/chrome-tts-blaze](http://go/chrome-tts-blaze)
for instructions on how to build the engine from source and get the
latest voice files.

When debugging, start Chrome from the command-line and set the
NACL_PLUGIN_DEBUG environment variable to 1 to print log messages to stdout.

If running on Chrome OS on desktop Linux, you can put the unpacked extension in
your downloads directory, and hide the existing TTS extension by temporarily
renaming /usr/share/chromeos-assets to something else. Then in
chrome://extensions you can enable developer mode and "load unpacked extension".
You must hide the existing TTS extension because extension keys must not be
duplicated, and ChromeOS will crash if you try to load the unpacked extension
while the built-in one is already loaded.

To test, use the [TTS Demo extension](https://chrome.google.com/webstore/detail/tts-demo/chhkejkkcghanjclmhhpncachhgejoel)
in Chromeos. This should automatically recognize the unpacked TTS extension
based on its manifest key. You can also use any site that uses a web speech API
demo. In addition, the Chrome Accessibility team has a 
[TTS Debug extension](https://chrome.google.com/webstore/detail/idllbaaoaldabjncnbfokacibfehkemd)
which can run several automated tests.

## Updating

First, follow the public
[Chromium OS Developer Guide](http://www.chromium.org/chromium-os/developer-guide) to check out the source.
At a minimum you'll need to create a chroot and initialize the build for your board.
You do not need to build everything from source.
You do need to start the devserver.

Next, flash your device to a very recent test build. Internally at Google
you can do this with the following command when the dev server is running,
where CHROMEBOOK_IP_ADDRESS is the IP address of your Chromebook already
in developer mode, and $BOARD is your Chromebook's board name.

```cros flash ssh://CHROMEBOOK_IP_ADDRESS xbuddy://remote/$BOARD/latest-dev/test```

Before you can make changes to PATTS, the first thing you need to run
(from the chroot) is call cros_workon with two relevant ebuilds:

```
cros_workon --board=$BOARD start chromiumos-assets
cros_workon --board=$BOARD start common-assets
```

From outside the root, from anywhere under your top-level ```<repo-dir>```, pull down the relevant sources:

```
repo sync
```

Again, outside the root, make sure you're in the ```<repo-dir>/src/platform/assets``` directory and run
```repo start``` to create a branch:

```
cd src/platform/assets
repo start <branch_name> .
```


The PATTS data files can be found in this directory:

```src/platform/assets/speech_synthesis/patts```

When updating the files, the native client files (nexe) need to be zipped.

Replace all of the files you need to update. You will probably not need
to update the manifest.json, tts_main.js or tts_controller.js, as these
are probably most up-to-date on ChromeOS and not google3. Look at recent
commit history on both platforms to determine what changes should be
pushed.

Commit your changes using git, then from the chroot, run:

```
emerge-$BOARD common-assets
cros deploy CHROMEBOOK_IP_ADDRESS common-assets
```

Note that you need to call cros_workon on both chromeos-assets and
common-assets. You will be changing files in chromeos-assets, but
to flash it onto your device, you need to emerge and deploy
common-assets.

After that, reboot your Chromebook and verify that speech works.

To upload the change, use repo upload, something like this:

```
git commit -a
  BUG=chromium:12345
  TEST=Write what you tested here
repo upload .
```

After submitting, inform the [Chrome Accessibility Team](mailto:chrome-a11y-core@google.com)
so that they can update their local copies of TTS per the
[Chromevox instructions](chromevox_on_desktop_linux.md).

## Ebuild

Note that sometimes you'll have to update the ebuild file that
takes the patts data files and installs them, unzipping the .nexe
files in the process.

For example, you'll need to edit the ebuild if you add or remove
a language code, or if you add or remove a file that needs to be
installed as part of the extension.

To update the ebuild, edit this file:

```
/third_party/chromiumos-overlay/chromeos-base/common-assets/common-assets-9999.ebuild
```

If you need to land changes to both common-assets and chromiumos-assets,
upload the changes separately and then make them depend on one another
using this syntax in the changelog:

```
CQ-DEPEND=CL:12345
```

Note that you can (and often should) have two changes depend on one another
so they'll land atomically.


