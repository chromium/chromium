# Security Considerations for Browser UI

Users expect to be able to use browsers to visit any website, including those
which they don't trust to deliver safe web content and code. One of the ways
that an unsafe website might try to harm a user is by attacking, abusing, or
manipulating the browser's own UI (aka the browser chrome). Browser UI therefore
has some special usable security considerations. For most browser UI, there are
two main attacks to think about: spoofing and clickjacking.

## Spoofing

Many aspects of browser security rely on the user understanding the difference
between trustworthy browser UI and untrustworthy web content. Spoofing refers to
when untrustworthy web content mimics browser UI such that the user can't tell
the difference. This could result in leaking sensitive data to an attacker,
confusing or tricking the user into taking an action that they wouldn't take
otherwise, or other more subtle forms of abuse.

Traditionally, web browsers try to maintain a line of death between trustworthy
browser chrome and untrustworthy web content. Trustworthy browser UI should
appear above the line of death, or at least be anchored to or overlap it.
[This blog post](https://textslashplain.com/2017/01/14/the-line-of-death/) contains
an excellent overview of this concept, including some of its shortcomings.

Maintaining an understandable and consistent distinction between browser UI and
web content is more of an art than a science. If you are building a new browser
UI surface, consider whether web content can spoof your UI, how convincingly
they might be able to do so, and what harm might come to the user as a result.

Here are some security principles to strive for to protect users and help them
build correct mental models about browser versus web content:

### Prefer negative security indicators to positive or neutral ones

Usually, attackers don't have much incentive to spoof negative security
indicators (like a warning) compared to a positive or neutral one. Positive
security indicators also have
[other usability problems](https://www.troyhunt.com/the-decreasing-usefulness-of-positive-visual-security-indicators-and-the-importance-of-negative-ones/).
Prefer to warn users when things are amiss, rather than reassure them about
neutral or positive security properties.

(Note that negative security indicators can be abused too! For example, some
tech support scam websites show fake Safe Browsing or certificate warnings to
encourage the user to believe that their computer is infected and they need to
purchase fraudulent tech support or antivirus software. However, there are many
other ways that these scams could try to convince the user that their computer
is broken, and generally negative indicators are a less fruitful target for
spoofing than positive ones.)

### Prefer existing UI/UX patterns; avoid introducing new ones

Reuse familiar patterns rather than introducing new ones to help users maintain
an accurate mental model of browser UI versus web content. For example, instead
of building a new browser UX pattern, can you model the functionality as a
normal webpage in a normal tab? Instead of introducing a new type of prompt or
modal dialog, can you send the user into Settings to make a choice there? The
fewer UI/UX patterns the user has to learn, the more likely that they will be
able to accurately discern trustworthy from untrustworthy content when it
matters most.

### Avoid mixing trustworthy with untrustworthy content; clearly demarcate untrustworthy content

We generally try to avoid showing attacker-controlled content in browser UI, and
when we do so, we constrain it and make it clear from context that it is not
content provided by the browser. For example, well-meaning people often propose
showing website-provided strings in permission prompts to help the user
understand why a website is asking for a particular permission. We avoid this
because attackers might abuse this functionality to trick the user into granting
a permission that they wouldn't otherwise grant.

When demarcating untrustworthy content in browser UI, consider what might happen
if the content is longer than expected or uses unexpected characters. Take
special care with [URLs](url_display/url_display_guidelines.md).

### Avoid occlusion of browser UI

For many forms of browser UI, spoofing is not as scary a risk as partial or full
occlusion. For example, a website that spoofs a browser permission prompt cannot
do too much harm, because the spoofed permission prompt will not cause the
permission to actually be granted. But consider an attacker who can cover the
origin shown in the permission prompt with an origin of their own choosing --
that is much scarier than fully spoofing the dialog, because the user might
choose to grant the permission based on a misunderstanding of which origin they
are interacting with. Another example is the fullscreen disclosure bubble which
tells users that they are in fullscreen mode and how to exit it; an attacker who
can occlude the fullscreen bubble is very powerful as there are no longer any
trustworthy pixels on the screen.

When introducing a new form of browser UI, take care to ensure that attacker
content can't occlude it, partially or fully.

## Clickjacking and keyjacking

Clickjacking is when an attacker tricks the user into clicking or interacting
with a UI element that they don't mean to interact with. Historically,
clickjacking was mainly considered an attack executed by one website against
another. For example, `attacker.com` iframes `victim.com` and covers up the
`victim.com` UI. `attacker.com` then tricks the user into clicking in a
particular location (e.g., by convincing them to play a game that incentivizes
them to click in a specific location), and at the last minute they remove the
occluding UI and the click falls through to the `victim.com` frame, causing the
user to take some unintentional action on `victim.com`. Keyjacking is a similar
attack except the user's keypress rather than click falls through to the occluded
UI. This category of attack is also sometimes called "UI redressing".

Clickjacking is mostly under control on the web, but it hasn't been
systematically tackled for browser UI, and many browser UI surfaces are
clickjackable. If a website can trick a user into clicking at a particular
location at a particular time, the attacker may be able to trigger some browser
UI (such as a prompt or dialog) to show up at that location at exactly the right
moment such that the user is tricked into interacting with the browser UI
without realizing it.

Not all browser UI surfaces are hardened against clickjacking/keyjacking, but
new ones should be if possible. Consider these possible defenses:

### Don't have a default-selected accept button

If your dialog or UI has a call-to-action triggered by a button that is
default-selected, the dialog is subject to keyjacking. An evil webpage can trick
a user into mashing or repeatedly hitting the Enter key, and then trigger your
dialog to show, causing the user to unknowingly accept. Users should have to
make an explicit selection on security- or privacy-sensitive browser UI
surfaces.
([Example](https://bugs.chromium.org/p/chromium/issues/detail?id=865202#c9))

### Require multiple clicks/gestures before an action is taken

For example, if introducing a new type of permission prompt, consider whether it
is feasible for the permission-granting flow to involve two clicks in two
different locations. Tricking the user into making two clicks in two locations
on browser UI is a lot harder than tricking the user into one click on a single
location.

### Introduce a short delay before the UI's call-to-action activates

If multiple clicks/gestures aren't feasible, consider introducing a short delay
between when the browser UI is shown and the call-to-action activates. For
example, if the user must click a button to grant a permission, introduce a
delay before the button becomes active once the permission prompt is
shown. Chrome uses short and long delays in various UI:

- For large security-sensitive browser surfaces like interstitials, three
seconds is typically considered a delay that is long enough to let the user
notice that the UI is showing without being too disruptive to the typical user
experience.

- For smaller UI surfaces such as dialog boxes, a shorter delay like 500ms can
be more practical. [`InputEventActivationProtector`](
ui/views/input_event_activation_protector.h) is a helper class that ignores UI
events that happen within 500ms of the sensitive UI being displayed.
