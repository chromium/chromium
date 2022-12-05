# ChromeOS Dictation

<!-- TODO(akihiroota): document the rest of Dictation. -->
Dictation is a ChromeOS accessibility feature that allows users to type and edit
text with their voice.

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
