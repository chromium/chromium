# How accessibility works

## Overview
In modern operating systems, accessibility can be thought of as the set of
features which allow for alternative interactions with the system. In practice,
it is all about letting users work in the best way they can, regardless of
ability or disability. Since users have almost an unbounded set of needs,
accessibility has to constantly keep up and adapt to include increasing numbers
of alternate interactions. It is this very challenge which gives accessibility
at the level of an operating system such complexity, depth, and interest.

For Chrome OS, this document will provide the reader with a technical survey of
the major system components to give a good foundation for additional
exploration.

![Chrome OS Accessibility Architecture: entities and how they relate to one
another.](figures/architecture.png)

At a glance, Chrome OS accessibility roughly decomposes into three major
entities, separated by large abstractions like process boundaries or api layers.

Through a slightly different lens, the three entities can also be thought of as the three major stakeholders in any accessibility interaction:
- accessibility feature: an assitive technology (AT) developer can produce services targeting a specific audience. For example, screen readers such as ChromeVox. The extension system serves as a rich runtime with apis to transform the user interface into something a user can use.
- frameworks: an operating system developer abstracts away low-level details to provide for a clean AT developer experience. In Chrome OS, this framework mainly comes by way of extension apis, though other internal api surfaces exist in C++ for Chromium developers as well. The apis are backed by libraries and executables run in native or sandboxed contexts.
- renderers: app developers and web page authors have their content executed and drawn on-screen. This is where all user interactions such as key events end up. The developers and authors here also expose accessibility information for an accessibility tree.

### Renderers
A renderer as used by this document, is any component that draws to the
screen. Some examples include Blink (for web content), ARC++ (for Android apps).

Each of these renderers store an in-memory representation of their user
interfaces. For Blink, this is the DOM or the render tree. For Android, this is
the application view tree.

In almost all modern renderers, there is an equivalent parallel structure, built
off of the source of truth UI tree, called the accessibility tree. It is here
where Chrome OS accessibility extracts all it needs.

The accessibility tree is constructed and updated as the user interface changes
in any way. Every single text change, scroll, load, and more gets
represented. Likewise, every type of data provided by either a developer or the
underlying framework is consumed and stored. This includes things like the
rendered text, styling information, bounding boxes, tree relationships, and much
more.

When this construction finishes or updates with new content, the renderer will
serialize the tree and prepare it for export out of process if needed. That
moves the data to the framework entity below.

For the web, PDF, and Android, renderers exist out of process. For the Aura
windowing system, along with its Views toolkit, the accessible tree lives in the
Ash process.

#### Deep dives
- [Chrome web accessibility](../browser/how_a11y_works.md)

### Frameworks
Here are a few key api surfaces on which accessibility depends.

#### Public apis
- [chrome.automation](https://developer.chrome.com/docs/extensions/reference/automation/)
- [chrome.tts](https://developer.chrome.com/docs/extensions/reference/tts/)
- [chrome.ttsEngine](https://developer.chrome.com/docs/extensions/reference/ttsEngine/)

#### Private apis (only available in component accessibility extensions)
- [chrome.accessibilityPrivate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/extensions/api/accessibility_private.json)
- [chrome.brailleDisplayPrivate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/extensions/api/braille_display_private.idl)
- [chrome.speechRecognitionPrivate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/extensions/api/speech_recognition_private.idl)

#### Libraries and system executables
- [BRLTTY](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/app-accessibility/brltty/)
- [Chrome OS text-to-speeech](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/app-accessibility/googletts/)
- [Espeak text-to-speech](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/main/app-accessibility/espeak-ng/)

#### Deep dives
- [How text-to-speech works on Chrome OS (coming soon)](how_tts_works.md).

### Accessibility features
See [How accessibility features work (coming soon)](how_a11y_features_work.md) for more details.
