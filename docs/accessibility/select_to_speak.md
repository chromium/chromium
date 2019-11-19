# Select to Speak (for developers)

Select to Speak is a Chrome OS feature to read text on the screen out loud.


There are millions of users who greatly benefit from some text-to-speech but
don’t quite need a full screen reading experience where everything is read
aloud each step of the way. For these users, whether they are low vision, 
dyslexic, neurologically diverse, or simply prefer to listen to text read
aloud instead of visually reading it, we have built Select-to-Speak. 

## Using Select to Speak

Go to Chrome settings, Accessibility settings, “Manage accessibility Features”,
and enable “Select to Speak”. You can adjust the preferred voice, highlight
color, and access text-to-speech preferences from the settings page.

With this feature enabled, you can read text on the screen in one of three ways:

- Hold down the Search key, then use the touchpad or external mouse to tap or
drag a region to be spoken

- Tap the Select-to-Speak icon in the status tray and use the mouse or
touchscreen to select a region to be spoken

- Highlight text and use Search+S to speak only the selected text.

Read more on the
[Chrome help page](https://support.google.com/chromebook/answer/9032490?hl=en)
under “Listen to part of a page”.

## Reporting bugs

Use bugs.chromium.org, filing bugs under the component
[UI>Accessibility>SelectToSpeak](https://bugs.chromium.org/p/chromium/issues/list?sort=-opened&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&q=component%3AUI%3EAccessibility%3ESelectToSpeak%20&can=2).

## Developing

*Select to Speak will be abbreviated STS in this section.*

### Code location

STS code lives mainly in three places:

- A component extension to do the bulk of the logic and processing,
chrome/browser/resources/chromeos/select_to_speak/

- An event handler, ash/events/select_to_speak_event_handler.h

- The status tray button, ash/system/accessibility/select_to_speak_tray.h

In addition, there are settings for STS in 
chrome/browser/resources/settings/a11y_page/manage_a11y_page.*

### Tests

Tests are in ash_unittests and in browser_tests:

```
out/Release/ash_unittests --gtest_filter=”SelectToSpeak*”
out/Release/browser_tests --gtest_filter=”SelectToSpeak*”
```
### Debugging

Developers can add log lines to any of the C++ files and see output in the
console. To debug the STS extension, the easiest way is from an external
browser. Start Chrome OS on Linux with this command-line flag:

```
out/Release/chrome --remote-debugging-port=9222
```

Now open http://localhost:9222 in a separate instance of the browser, and
debug the Select to Speak extension background page from there.

## How it works

Like [Chromevox](chromevox.md), STS is implemented mainly as a component
Chrome extension which is always loaded and running in the background when
enabled, and unloaded when disabled. The only STS code outside of the
extension is an EventRewriter which forwards keyboard and mouse events to
the extension as needed, so that the extension can get events systemwide.

The STS extension does the following, at a high level:

1. Tracks key and mouse events to determine when a user has either:

    a. Held down “search” and clicked & dragged a rectangle to specify a
    selection

    b. Used “search” + “s” to indicate that selected text should be read

    c. Has requested speech to be canceled by tapping ‘control’ or ‘search’
    alone

2. Determines the Accessibility nodes that make up the selected region

3. Sends utterances to the Chrome Text-to-Speech extension to be spoken

4. Tracks utterance progress and updates the focus ring and highlight as needed.

### Select to Speak extension structure

Most STS logic takes place in
[select_to_speak.js](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/select_to_speak/select_to_speak.js).

#### User input

Input to the extension is handled by input_handler.js, which handles user
input from mouse, keyboard, and touchscreen events. Most logic here revolves
around keeping track of state to see if the user has requested text using
one of the three ways to activate the feature, search + mouse, tray button 
+ mouse, or search + s.

#### Determining selected content

Once input_handler determines that the user did request text to be spoken,
STS must determine which part of the page to read. To do this it requests
information from the Automation API, and then generates a list of
AutomationNodes to be read.

##### With mouse or touchpad

select_to_speak.js fires a HitTest to the Automation API at the center of
the rect selected by the user. When the API gets a result it returns via
SelectToSpeak.onAutomationHitTest_. This function walks up from the hit
test node to the nearest container to find a root, then back down through
all the root’s children to find ones that overlap with the selected rect.
Walking back down through the children occurs in NodeUtils.findAllMatching,
and results in a list of AutomationNodes that can be sent for speech.

##### With search + s

select_to_speak.js requests focus information from the Automation API. The
focus result is sent to SelectToSpeak.requestSpeakSelectedText_, which
uses Automation selection to determine which nodes are selected. The 
complexity of logic here is converting between Automation selection and
its deep equivalent, i.e. from parent nodes and offsets to their leaves.
This occurs in NodeUtils.getDeepEquivalentForSelection. When the first and
last nodes in selection are found, SelectToSpeak.readNodesInSelection_ is
used to determine the entire list of AutomationNodes which should be sent 
for speech.

#### Speaking selected content

SelectToSpeak.startSpeechQueue_ takes a list of AutomationNodes, determines
their text content, and sends the result to the Text to Speech API for
speech. It begins by mapping the text content of the nodes to the nodes 
themselves, so that STS can speak smoothly across node boundaries (i.e.
across line breaks) and follow speech progress with a highlight. The mapping
between text and nodes occurs in repeated calls to 
ParagraphUtils.buildNodeGroup to build lists of nodes that should be spoken 
smoothly.


Each node group is sent to the Text to Speech API, with callbacks to allow 
for speech progress tracking, enabling the highlight to be dynamically 
updated with each word.

#### Highlighting content during speech

On each word boundary event, the TTS API sends a callback which is handled
by SelectToSpeak.onTtsWordEvent_. This is used to check against the list of
nodes being spoken to see which node is currently being spoken, and further 
check against the words in the node to see which word is spoken.

#### Edge cases

STS must also handle cases where:

- Nodes become invalid during speech, i.e. if a page was closed. Speech
should continue, but highlight stops.

- Nodes disappear and re-appear during speech (a user may have switched
tabs and switched back, or scrolled). Highlight should resume.

This occurs in SelectToSpeak.updateFromNodeState_.

### Communication with SelectToSpeakTray

STS runs in the extension process, but needs to communicate its three states 
(Inactive, Selecting, and Speaking) to the STS button in the status tray.
It also needs to listen for users requesting state change using the
SelectToSpeakTray button. The STS extension uses the AccessibitilityPrivate
method onSelectToSpeakStateChanged to inform the SelectToSpeakTray of a
status change, and listens to onSelectToSpeakStateChangeRequested to know 
when a user wants to change state. The STS extension is the source of truth
for STS state.

### Special case: Google Drive apps

Google Drive apps require a few work-arounds to work correctly with STS. 

- Any time a Google Drive document is loaded (such as a Doc, Sheet or Slides
document), the script
[select_to_speak_gdocs_script](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/select_to_speak/select_to_speak_gdocs_script.js?q=select_to_speak_gdocs_script.js+file:%5Esrc/chrome/browser/resources/chromeos/select_to_speak/+package:%5Echromium$&dr)
must be executed to remove aria-hidden from the content container.

- Using search+s to read highlighted text uses the clipboard to get text data
from Google Docs, as selection information may not be available in the
Automation API. This happens mostly in input_handler.js.

## For Googlers

For more, Googlers could check out the Select to Speak feature design docs
for more details on design as well as UMA.

- Overall product design, [go/select-to-speak-design](go/select-to-speak-design)

- On-Screen UI for touch and tablet modes, 
[go/chromeos-sts-on-screen-ui](go/chromeos-sts-on-screen-ui)

- Reading text at keystroke,
[go/chromeos-sts-selection-keystroke](go/chromeos-sts-selection-keystroke)

- Reading text at keystroke in Google Drive apps, [go/sts-selection-in-drive](go/sts-selection-in-drive)

- Per word highlighting,
[go/chrome-sts-sentences-and-words](go/chrome-sts-sentences-and-words) and
[go/chromeos-sts-highlight](go/chromeos-sts-highlight)
