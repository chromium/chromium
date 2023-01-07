# The eSpeak-NG speech synthesis engine on Chrome OS

Chrome OS comes with a port of the open-source eSpeak-NG speech synthesis
engine. eSpeak-NG is lower quality than Google's "PATTS" speech engine,
but it's faster, uses less resources, and supports more languages.

[Read more about Text-to-Speech in Chrome](../browser/tts.md).

[See also Google's "PATTS" speech engine](patts.md).

## About eSpeak-NG

eSpeak-NG is an open-source project, released under the
[GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html) license.
The current home of the project is on GitHub:

  https://github.com/espeak-ng

NG stands for Next Generation. It's a fork of the eSpeak engine created by
Jonathan Duddington.

Eitan Isaacson of Mozilla wrote the initial port of eSpeak to JavaScript
using emscripten in April 2015. Alberto Pettarin adapted that to work
with eSpeak-NG in October 2016.

The Chrome OS port is heavily based on the emscripten port, but targets
WebAssembly instead of asm.js, and uses a completely rewritten JavaScript
glue layer that implements Chrome's
[TTS Engine Extension API](https://developer.chrome.com/extensions/ttsEngine)
and outputs audio using an AudioWorklet from the Web Audio API.

## Why we include eSpeak-NG in Chrome OS

There are two reasons we include eSpeak-NG in Chrome OS:

1. To maximize our language coverage for text-to-speech and ensure
   that Chrome OS is accessible to as many users as possible out of the box.
2. As a secondary goal, to provide an alternative speech engine for screen
   reader users that's maximally responsive and works at the highest rates of
   speed.

## Building from source

The source code to the Chrome OS port of eSpeak-NG can be found here:

  https://chromium.googlesource.com/chromiumos/third_party/espeak-ng

All of the Chrome-specific changes are in the "chrome" branch. 

To get the source code locally:

1. Clone the repository
```
git clone https://chromium.googlesource.com/chromiumos/third_party/espeak-ng
```
2. Enter the espeak-ng directory
```
cd espeak-ng/
```
3. Switch to the "chrome" branch
```
git checkout chrome
```

From there,
[README.chrome](https://chromium.googlesource.com/chromiumos/third_party/espeak-ng/+/chrome/README.chrome)
has instructions on building eSpeak.

## Releasing a new version of eSpeak for Chrome OS

First, you should push any changes to the git repository:
(https://chromium.googlesource.com/chromiumos/third_party/espeak-ng).

As eSpeak-NG is licensed under the GPL, Chrome OS should never include any
changes to this project that haven't been committed to the Git repository first.
Make sure that the chrome-extension directory is complete and ready to use
as-is; in particular if changes were made to the native code, be sure to fully
compile using emscripten and copy and generated wasm files to the
chrome-extension/js directory.

Remember, all of the build instructions are in README.chrome in the "chrome"
branch. To test, open chrome://extensions in Chrome, enable Developer mode,
click Load unpacked, and point it to the espeak-ng/chrome-extension directory.

To update the package on Chrome OS, first export a tarball by running this
from inside the espeak-ng directory:

```
git archive chrome --prefix=espeak-ng/ | gzip > espeak-ng-1.49.3.2.tar.gz
```

Version number: the first three components (1.49.3 in the example above)
should match the eSpeak-NG version in CHANGELOG.md, and the fourth component
should be incremented with each new release of the Chrome OS port for
that version.

Next, upload this file to chromeos-localmirror/distfiles and make it
world-readable (Googlers only):

  1. Visit https://pantheon.corp.google.com/storage/browser/chromeos-localmirror/distfiles/?pli=1
  2. Click "Upload files" and select your tarball
  3. Select the uploaded file, and from the More menu, choose
     "Edit permissions"
  4. Click Add item, then enter User -> allUsers -> Reader
  5. Save

The next steps require that you have the full Chrome OS source code
checked out and you're in your chroot. See the Chromium OS Developer Guide
for instructions:

https://chromium.googlesource.com/chromiumos/docs/+/main/developer_guide.md


espeak-ng is in this directory:
```
src/third_party/chromiumos-overlay/app-accessibility/espeak-ng
```

Rename the ebuild to match the version number of the tarball you uploaded.
The version number must match exactly! Then, add the new renamed file
to git. For example:

```
mv espeak-ng-1.49.3.1.ebuild espeak-ng-1.49.3.2.ebuild
git add espeak-ng-1.49.3.2.ebuild
```

Next, rebuild the manifest:

```
ebuild espeak-ng-1.49.3.2.ebuild manifest
```

To test it, use emerge to rebuild this package, and cros deploy to
deploy that package to an attached Chrome OS device, for example:

```
emerge-${BOARD} espeak-ng
cros deploy CHROMEBOOK_IP_ADDRESS espeak-ng
```

To upload the change for review, use 'repo start' to start making changes
in this package, then commit the change to git, ensuring that you're
changing both the ebuild and manifest file and adding appropriate Bug:
and Test: lines, then use 'repo upload' to upload the change for review.

## Running on Chrome OS on Desktop Linux

See [Chromevox on Desktop Linux](chromevox_on_desktop_linux.md#speech) for more
how to use eSpeak's speech engine on Chrome OS emulated on desktop Linux.
