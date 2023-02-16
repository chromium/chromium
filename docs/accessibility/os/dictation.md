# ChromeOS Dictation

Dictation is a ChromeOS accessibility feature that allows users to type and edit
text with their voice.

## Summary

### User flow

Dictation can be toggled on in two ways: by pressing Search + D or by pressing the
microphone icon (which has an accessibility label of "toggle dictation") in the
status tray. Once toggled, Dictation starts speech
recognition and the user can start speaking. If a command is recognized, then
it will execute the command to the best of its ability; otherwise it will input
the recognized text as-is. Dictation can be turned off in the same two ways
mentioned above; it will automatically turn off if no speech has been
recognized within a short period of time. Lastly, Dictation can only be used
when focus is on an editable field (textarea, input, contenteditable, etc).

### Technical overview

Dictation is implemented primarily as a chrome extension in JavaScript. It also
has a few C++ components, such as the UI and APIs that it uses. After receiving
a toggle, the Dictation extension starts speech recognition via the
chrome.speechRecognitionPrivate API. The API forwards all speech recognition
results to the Dictation extension, where it parses text to see if it matched a
command. If a command was detected, then it will verify and execute the
command to the best of its ability; otherwise, it will input the recognized text
using input method editor (IME) APIs. Throughout this flow, the main Dictation
object uses the chrome.accessibilityPrivate API to update the Dictation UI
(which lives in C++) so that the user is aware of Dictation's internal state.

It's also worth noting that Dictation utilizes two [DLCs](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/dlcservice/docs/developer.md): Pumpkin and SODA.
Pumpkin is a semantic parser that allows Dictation to extract meaning out of
recognized text. SODA stands for "speech on-device API" and turns the user's
speech into text on-device (without sending it to a Google server). When Pumpkin
isn't available or fails to download, Dictation falls back to regex-based
speech parsing. Similarly, when SODA isn't available, Dictation falls back to
network speech recognition.

## Code structure

The majority of Dictation code lives in the [dictation/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/accessibility_common/dictation/) extension directory.
There's also a small amount of C++ code, most of which is for the Dictation UI.

### Dictation JavaScript

The `dictation/` extension directory is broken into a few subdirectories
organized by functionality:

* The `parse/` directory contains all code related to speech parsing, which
is the process of turning recognized text into a command. Dictation currently
utilizes two parsing strategies: regex-based, which uses regular expressions to
match text to known commands, and Pumpkin-based, which uses a semantic parser
developed by Google (see more about Pumpkin below).

* The `macros/` directory contains all code for macro (also known as a command)
implementation.

All other Dictation JavaScript code is located in the main `dictation/`
directory. Some noteworthy classes are:

* `Dictation`, which is the main object. It handles setup/teardown, interacts
with APIs like chrome.speechRecognitionPrivate and chrome.settingsPrivate, and
owns many other essential classes.

* `InputController`, which handles all interaction with editable fields. In its
current form, it uses various IME APIs to enter text into the editable, as well
as listen to changes in the editable value. It's also responsible for
calculating the data about the focused editable node, the current value, the
selection start, and selection end. Lastly, it implements many of the editing
commands supported by Dictation.

* `UIController`, which handles all interaction with the Dictation UI. Since the
Dictation UI is implemented as a View in C++, it uses the
chrome.accessibilityPrivate API to manipulate the UI. Generally, all changes to
the UI should go through the UIController.

* `FocusHandler`, which tracks the currently focused node using the automation
API. Dictation can only work on editable nodes, so FocusHandler is used mostly
to check this precondition (and also to access to the node's accessibility
data).

* `LocaleInfo`, which is the source of truth for the Dictation locale and whether
or not certain behaviors are supported in the current locale.

### Dictation C++

* `DictationBubbleController` manages the Dictation UI from the C++ side and
provides an entry point for updating/changing the UI.

* `DictationBubbleView` is the actual implementation of the Dictation UI.

* `Dictation`, which used to contain the feature's implementation before it
was moved to JavaScript. It now contains a small amount of logic around
supported locales.

* `AccessibilityController` exposes several Dictation-related APIs, mostly
around updating the UI and showing notifications.

* `AccessibilityManager` contains a fair amount of Dictation logic, specifically
around setting up/tearing down the extension, managing DLC downloads, and
showing notifications to the user.

### Testing

* See the `dictation/` extension directory for all JavaScript extension tests.

* See [dictation_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/accessibility/dictation_browsertest.cc?q=dictation_browsertest.cc&ss=chromium%2Fchromium%2Fsrc) for C++ integration tests. Note that these
tests hook into a JavaScript class called `DictationTestSupport` and allows the
C++ tests to execute JavaScript or wait for information to propagate to the
JavaScript side before continuing.

Two C++ helper classes that are worth noting are `DictationBubbleTestHelper`,
which allows tests to query the state of the Dictation UI or wait for it
to reach a certain state, and `SpeechRecognitionTestHelper`, which allows tests
to easily interact with speech recognizers.

## Semantic parsing with Pumpkin

### Background

Dictation utilizes a semantic parser called Pumpkin to extract meaning and
intent out of text and allows us to turn recognized text into commands. To use
Pumpkin in Dictation, a few steps had to be taken:

1. A Web Assembly port was created so that Pumpkin could be run in JavaScript.
It currently lives in Google3. For the specific build rule for this, see this
[build file](https://source.corp.google.com/piper///depot/google3/speech/grammar/pumpkin/api/BUILD;l=154).

2. Pumpkin and associated config files take up roughly 5.9MB of space (estimate
generated in December 2022). Adding this much memory overhead to rootfs was not
a feasible option, so we added a [DLC for Pumpkin](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/app-accessibility/pumpkin/pumpkin-9999.ebuild?q=file:pumpkin%20file:ebuild&ss=chromiumos)
so that it could be downloaded and used when needed. We added a
[script in Google3](https://source.corp.google.com/piper///depot/google3/chrome/chromeos/accessibility/dictation/update_pumpkin_dlc.sh?q=update_pumpkin_dlc)
that would quickly generate the DLC and upload it to Google Cloud Storage
whenever it needed to be updated.

3. We added logic in Dictation that would initiate a download of the Pumpkin
DLC. Dictation uses the chrome.accessibilityPrivate.installPumpkinForDictation()
API to initiate the download. Once the DLC is downloaded, the
AccessibilityManager reads the bytes of each Pumpkin file and sends them back to
the Dictation extension. Lastly, the extension spins up a new sandboxed context
to run pumpkin in.

### Adding a new Pumpkin command

1. Update the Dictation [semantic_tags.txt](https://source.corp.google.com/piper///depot/google3/chrome/chromeos/accessibility/dictation/semantic_tags.txt)
file with the tags that correspond to the new commands you’d like to add. All
Dictation commands can be found in [macro_names.js](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/accessibility_common/dictation/macros/macro_names.js).
See the "creating patterns files" section of the [Dictation google3 documentation](https://source.corp.google.com/piper///depot/google3/chrome/chromeos/accessibility/dictation/README.md)
for more information on semantic tags.
2. Update the Pumpkin DLC according to the [documentation](https://source.corp.google.com/piper///depot/google3/chrome/chromeos/accessibility/dictation/README.md).
3. Add the newest ```pumpkin-<version>.tar.xz``` file to the chromium codebase
for testing purposes. A copy of the tar file should be placed in your root
directory (e.g. ~/pumpkin-3.0.tar.xz) if you followed the documentation above.
4. Add tests for the new commands to [dictation_pumpkin_parse_test.js](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/accessibility_common/dictation/parse/dictation_pumpkin_parse_test.js).
These should fail initially.
5. Make your tests pass by connecting the new commands to
[pumpkin_parse_strategy.js](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/accessibility_common/dictation/parse/pumpkin_parse_strategy.js).
6. (Optional, but strongly recommended) Add tests to [dictation_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/accessibility/dictation_browsertest.cc)
to get end-to-end test coverage.

Note: It's important that we never remove semantic tags from the Pumpkin DLC
because we want to avoid backwards compatibility issues (we never want to
regress any commands).
