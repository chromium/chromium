# The eSpeak-NG speech synthesis engine on ChromeOS

ChromeOS comes with a port of the open-source eSpeak-NG speech synthesis
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

The ChromeOS port is heavily based on the emscripten port, but targets
WebAssembly instead of asm.js, and uses a completely rewritten JavaScript
glue layer that implements Chrome's
[TTS Engine Extension API](https://developer.chrome.com/extensions/ttsEngine)
and outputs audio using an AudioWorklet from the Web Audio API.

## Why we include eSpeak-NG in ChromeOS

There are two reasons we include eSpeak-NG in ChromeOS:

1. To maximize our language coverage for text-to-speech and ensure
   that ChromeOS is accessible to as many users as possible out of the box.
2. As a secondary goal, to provide an alternative speech engine for screen
   reader users that's maximally responsive and works at the highest rates of
   speed.

## Building, deploying, and releasing from source

The source code to the ChromeOS port of eSpeak-NG can be found in
[google3](http://cs/google3/third_party/espeak_ng/)

The google3 repository automatically syncs its changes to the [googlesource
repository](https://chromium.googlesource.com/chromiumos/third_party/espeak-ng)
via copybara. All of the Chrome-specific changes are in the "chrome" branch.

For instructions for building, deploying, and releasing eSpeak-NG see the
[README in google3](http://cs/google3/third_party/espeak_ng/README.chrome)

## Running on ChromeOS on Desktop Linux

See [Chromevox on Desktop Linux](chromevox_on_desktop_linux.md#speech) for more
how to use eSpeak's speech engine on ChromeOS emulated on desktop Linux.
