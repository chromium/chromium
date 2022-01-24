The terms "activation" and "focus" are often used in the chromium code base with
different semantics depending on platform [windows, macOS, etc.] or component
[e.g. views, blink]. For legacy reasons, sometimes even in a given component
[e.g. views] the semantics are unclear.

The goal of this document is to outline the intended definition of these terms.
This is not a one-size fits all solution. It fails to capture some
platform-specific nuances -- e.g. key vs main windows on macOS -- but it
provides a common lexicon for discussion.

Activation: Only applies to windows - boundary that applies to focusable
elements.

Focus: Only applies to leaf controls - Keyboard focus is sent and generally
processed by the targeted leaf element.

Activation and focus are not interchangeable. Windows are required to managed
focus within themselves (adjusting as necessary, even when not activated!).
Likewise, each tab/web-contents remembers its focused leaf control, even when
the tab is not showing because it's backgrounded.

Focusing a window is not a valid concept -- usually the intention is activation.

Focusing a control within a window is fine, and will also activate the window.

Activating a window brings it to the front, and gives the window priority [but
not exclusivity] for handling input events.

