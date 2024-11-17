# Fenced frame: Permissions Policies

## How to use this document

A fenced frame is an embedded context that enforces strict boundaries between it
and its embedder. Fenced frames are not allowed to communicate information to
and from their embedders (i.e. using something like `window.postMessage()`), nor
are they allowed to learn information about their context that can help them
form a fingerprint of where they're embedded.

This document serves as an audit for every permissions-backed feature to
determine whether it's safe to enable inside of a fenced frame. To determine
that, we need to answer the following questions:

1. Will enabling this feature allow an embedder to send information into a
   fenced frame? For example, can the embedder modify some global state that a
   fenced frame can later observe? We refer to this in the audit as an
   "infiltration risk".
1. Will enabling this feature allow a fenced frame to send information back to
   its embedder? We refer to this as an "exfiltration risk".
1. Will enabling this feature allow a fenced frame to learn about the context
   it's embedded in and create a fingerprint? As an example, the Geolocation API
   will give a fenced frame access to information about the user's location,
   which is a high-entropy fingerprint. We refer to this as a "fingerprinting
   risk". When referring to fingerprinting in this document, we are referring to
   [active
   fingerprinting](https://www.w3.org/TR/fingerprinting-guidance/#active-0)
   specifically, as all of these require the use of JavaScript to read the
   relevant properties the APIs expose.

**If you're adding a new permissions-backed feature, please do the following:**

1. Answer the 3 questions above to determine the infiltration, exfiltration, and
   fingerprinting risks for your API, if any exist.
1. Add a new entry in this document with your feature, what risks they carry,
   and a brief justification either outlining the risks or explaining why the
   API carries no risk.

Please feel free to reach out to
`third_party/blink/renderer/core/html/fenced_frame/OWNERS` with any questions
you have.

## Permissions for fenced frames created with Protected Audience

Protected Audience-created fenced frames have the privacy
guarantee that no information can flow from the embedder into the fenced frame.
Permissions-backed features pose a risk for 2 reasons:

1. The embedder can enable and disable a specific combination of features. The
   fenced frame can then test for those features to create a fingerprint of the
   context it's in, allowing up to allowing up to 1 bit per feature (up to 84
   bits, as of August 2023) to be infiltrated into the fenced frame.
2. A lot of features allow for data inflow, data outflow, and their own unique
   fingerprinting vectors, circumventing existing protections we have in place
   for fenced frames.

To mitigate these risks, we only allow Protected Audience fenced frames to load
with specific features enabled. These features ***must*** be enabled for the
fenced frame's origin. If any of the required features are disabled, the fenced
frame will not load.

See: `kFencedFrameFledgeDefaultRequiredFeatures` in
`third_party/blink/public/common/frame/fenced_frame_permissions_policies.h`.

## Permissions for Fenced frames created with selectURL()

selectURL-created fenced frames can contain information from the embedder by
having the embedder add arbitrary data to the URLs that the frame is navigated
to. Because of this, it is acceptable to have information flow in from the
embedder to the fenced frame via permission backed features.

However, stopping data outflow from the fenced frame to the embedder is still
part of the privacy story. Many permissions-backed APIs can be used to
exfiltrate data out of a fenced frame, so they cannot be enabled. To be safe, we
are currently only allowing a few permissions-backed features to be enabled that
are required for functionality purposes.

See: `kFencedFrameSharedStorageDefaultRequiredFeatures` in
`third_party/blink/public/common/frame/fenced_frame_permissions_policies.h`.

## Permissions for fenced frames with unpartitioned data access

Third-party cookie deprecation breaks useful website features like personalized
payment buttons. Fenced frames offer a potential solution by allowing controlled
access to cross-site user data stored in Shared Storage. To protect privacy,
fenced frames must disable external communications (specifically, anything that
can send data out of a fenced frame) before accessing this sensitive data.
Otherwise, the sensitive information can be exfiltrated, creating a data leak.

When these fenced frames are constructed using a URL provided by the embedder
and can contain information from the embedder in the URL, it is acceptable to
have information flow in from the embedder to the fenced frame via permission
backed features. However, Permissions-backed APIs pose the challenge of data
outflow, since many of them can allow for the exact kind of data exfiltration
that must be prevented to keep the unpartitioned data safe. To mitigate that
risk, we are currently allowing a small subset of permissions-backed features to
be enabled. This can be expanded in the future if more use cases are found that
require other features.

See: `kFencedFrameAllowedFeatures` in
`third_party/blink/public/common/frame/fenced_frame_permissions_policies.h`.

## Permissions policy-based features audit

Below is an audit of all permissions-backed features (as of August 2023) and
whether enabling them for fenced frames (that can allow embedder information to
be transferred to the FF), if needed in the future, is safe or will pose a data
exfiltration risk:

Legend:

* ✅: No risk/issues
* 🔻: Infiltration risk
* 🔺: Exfiltration risk
* 🖐️: Fingerprinting risk
* ❌: Usability issues

### ✅ Autoplay: no risk
*Feature: kAutoplay*

Media playing in a subframe cannot be observed by its embedder, and an embedder
cannot use this to influence how the media plays (outside of the feature being
enabled to begin with). While allowing autoplay has the potential to be
annoying, there is no risk of data exfiltration or fingerprinting by allowing
this.

### 🔻🖐️ Camera & Microphone: infiltration/fingerprinting risk, no exfiltration risk
*Feature: kCamera, kMicrophone*

These involve interacting with a
[`MediaStream`](https://developer.mozilla.org/en-US/docs/Web/API/MediaStream)
object. Any interactions with MediaStreams only affect that instance, and will
not affect other MediaStreams in other contexts that are pointing to the same
input device.

Use of the camera or microphone itself poses both a data infiltration risk. An
embedding page could encode data into a sound that is picked up by the
microphone and sent to the fenced frame.

There is also a fingerprinting risk with these APIs. The embedding page and
fenced frame could take a video or audio recording and use that as a fingerprint
to join data server-side.

There is no additional exfiltration risk introduced with enabling this API in a
fenced frame.

### ✅ Encrypted Media: no risk
*Feature: kEncryptedMedia*

This feature deals with encrypting and decrypting media. The media information
cannot be passed anywhere using this feature; it instead would have to rely on
other methods of communication if it wants to exfiltrate this data.

### 🔻🔺🖐️ Fullscreen: infiltration/exfiltration/fingerprinting risk
*Feature: kFullscreen*

A
[`fullscreenchange`](https://developer.mozilla.org/en-US/docs/Web/API/Document/fullscreenchange_event)
event that originates from a fenced frame is observable by its embedder and vice
versa. Entering fullscreen requires user gesture, so this is less of a concern,
but can still be used in conjunction with timing to leak information.

[`Document.fullscreenEnabled`](https://developer.mozilla.org/en-US/docs/Web/API/Document/fullscreenEnabled)
can be used as a fingerprinting vector, since it returns information about
whether fullscreen is allowed in the browser/page.

### 🖐️ Geolocation: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kGeolocation*

The API is read-only.
[`getCurrentPosition()`](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation/getCurrentPosition)
and
[`watchPosition()`](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation/watchPosition)
do not modify anything about how geolocation works internally, so those calls
are not observable by other contexts.

Information about a user's location can be used for fingerprinting.

### 🔻🔺🖐️ I/O: infiltration/exfiltration/fingerprinting risk
*Feature: kSerial, kUsb, kBluetooth, kMidiFeature, kHid*

Arbitrary data can be sent out to external devices. If some other context (like
the embedding frame) connects to that same device, the device can then relay
that arbitrary data to the context, allowing for arbitrary data transfer.

The list of devices connected to the computer (through
[`getPorts()`](https://developer.mozilla.org/en-US/docs/Web/API/Serial/getPorts),
[`getDevices()`](https://developer.mozilla.org/en-US/docs/Web/API/USB/getDevices),
etc...) can be used as a fingerprinting vector.

### 🔻🔺🖐️ AR/VR: infiltration/exfiltration/fingerprinting risk
*Feature: kWebXr*

Arbitrary data can be sent to a VR headset, which, in turn, can encode and send
that data via its user input data to another context. Realistically, this attack
will never happen, but it is theoretically possible.

Information about a connected headset + AR capabilities can be used as a
fingerprinting vector.

### 🔻🔺🖐️ Smart Card: infiltration/exfiltration/fingerprinting risk
*Feature: kSmartCard*

The [readme for the smart card
feature](https://github.com/WICG/web-smart-card/blob/main/README.md#cross-origin-communication)
explicitly outlines a possible attack where one context can write arbitrary data
to a smart card, and another context can read that data from the smart card.

The names of the smart cards retrieved with `navigator.smartCard.getReaders()`
can be [used as a fingerprinting
vector](https://github.com/WICG/web-smart-card/blob/main/README.md#fingerprinting).

### ❌ Payment: no risk, usability issues
*Feature: kPayment*

While arbitrary data can be put into the [details parameter of a
PaymentRequest()
constructor](https://developer.mozilla.org/en-US/docs/Web/API/PaymentRequest/PaymentRequest),
the payment request won’t be allowed once
`window.fence.disableUntrustedNetwork()` is resolved. However, the main use case
for this API in a subframe requires direct communication between the embedder
and the subframe. Because fenced frames explicitly disallow this kind of
communication, the Payment Request API will be broken inside of fenced frames if
we enable them. Therefore, it should not be allowed.

### 🔻🔺 Sync XMLHttpRequest: infiltration/exfiltration risk, no fingerprinting risk
*Feature: kSyncXHR*

This feature determines if a synchronous XML request can be made. Without
network cutoff, this allows information to freely flow between the fenced frame
and arbitrary servers. Once `window.fence.disableUntrustedNetwork()` is
resolved, these requests should not succeed.

There are no API methods that can be used for fingerprinting.

### 🖐️ Sensor: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kAccelerometer, kGyroscope, kMagnetometer*

All properties of the
[`Sensor`](https://developer.mozilla.org/en-US/docs/Web/API/Sensor) and its
derived classes are either read only, or only affect that Sensor instance (such
as [`start()`](https://developer.mozilla.org/en-US/docs/Web/API/Sensor/start)
and [`stop()`](https://developer.mozilla.org/en-US/docs/Web/API/Sensor/stop)).
This cannot be used to pass information between an embedder and its fenced
frame.

Information about the availability of sensors can be used to fingerprint the
browser the fenced frame was loaded in.

### 🖐️🔻 Ambient Light Sensor: fingerprinting/infiltration risk, no exfiltration risk
*Feature: kAmbientLightSensor*

Similar to the 3 other sensor APIs above, information about the availability of
an ambient light sensor can be used for fingerprinting.

An ambient light sensor can detect changes in brightness from the contents on a
device's screen when the device is in a dark room, so an embedder could use
changes in content brightness to encode information that the frame can then
read.

### 🔻🔺 Picture in Picture: infiltration/exfiltration risk
*Feature: kPictureInPicture*

There can only be 1 PIP window open at a time. If a different context requests
PIP, the media currently in PIP mode will be removed from PIP. This change can
be observed by checking the
[`document.pictureInPictureElement`](https://developer.mozilla.org/en-US/docs/Web/API/Document/pictureInPictureElement)
property, which will become null if its PIP media is removed from PIP.

### ✅ Vertical Scroll: no risk
*Feature: kVerticalScroll*

This simply allows or disallows specific subframes from interfering with
vertical scrolling. Since the act of scrolling is an entirely user-initiated
action, this can’t be exploited to exfiltrate information.

Aside from the previously-mentioned 1-bit fingerprinting vector from enabling
the permissions policy, there are no additional fingerprinting concerns.

### 🔺🔻🖐️ Screen Wake Lock: infiltration/exfiltration/fingerprinting risk
*Feature: kScreenWakeLock*

Changes to
[`Navigator.wakeLock`](https://developer.mozilla.org/en-US/docs/Web/API/WakeLock)
are viewable externally. As an example, [load this
site](https://whatwebcando.today/wake-lock.html) and toggle navigator.wakeLock
on in the Live Demo. Then, [load the MPArch demo
page](https://mparch.glitch.me/?url=.%2Fwake_lock.html), click “Add IFrame”,
then in the iframe click “release”. On the first site, the “Wake Lock status” in
the Live Demo will switch to “released externally”.

The timestamps of the `wakeLock` operations can be used for fingerprinting.

### 🖐️ Idle Detection: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kIdleDetection*

All
[`IdleDetector`](https://developer.mozilla.org/en-US/docs/Web/API/IdleDetector)
methods are either read only or only affect that instance of IdleDetector.

Checking the time that a
[`userState`](https://developer.mozilla.org/en-US/docs/Web/API/IdleDetector/userState)
or
[`screenState`](https://developer.mozilla.org/en-US/docs/Web/API/IdleDetector/screenState)
changes can be used as a fingerprinting vector.

### 🔻 Execution: infiltration risk, no exfiltration/fingerprinting risk
*Feature: kExecutionWhileOutOfViewport, kExecutionWhileNotRendered*

These features enable or disable code execution under certain conditions.
*kExecutionWhileNotRendered* can be used to allow a fenced frame to learn about
its embedder (specifically whether the fenced frame is being rendered or not,
which can be controlled by the embedder). The embedder isn’t able to learn
anything about a fenced frame through these flags. Note that viewability is
observable via the [intersection observer
API](https://developer.mozilla.org/en-US/docs/Web/API/Intersection_Observer_API),
which we [allow for utility
reasons](https://github.com/WICG/fenced-frame/blob/master/explainer/integration_with_web_platform.md#viewability).

### 🔻🔺❌ Focus: infiltration/exfiltration risk, usability issues
*Feature: kFocusWithoutUserActivation*

Enabling this will have no effect on fenced frames since they will always gate
focus on user activation. Allowing a developer to enable this feature could lead
to confusion, so this should be disabled.

### 🖐️ Client Hints: fingerprinting risk
*Feature: kClientHintDPR, kClientHintDeviceMemory, kClientHintDownlink,
kClientHintECT, kClientHintRTT, kClientHintUA, kClientHintUAArch,
kClientHintUAModel, kClientHintUAPlatform, kClientHintViewportWidth,
kClientHintWidth, kClientHintUAMobile, kClientHintUAFullVersion,
kClientHintUAPlatformVersion, kClientHintPrefersColorScheme,
kClientHintUABitness, kClientHintViewportHeight, kClientHintUAFullVersionList,
kClientHintUAWoW64, kClientHintSaveData, kClientHintPrefersReducedMotion,
kClientHintUAFormFactor*

This allows a fenced frame to learn about the device it’s on, but no information
will flow back to the embedder. Data is only sent at navigation time, as the
client hints live in the HTTP request headers. This can be used for
fingerprinting at navigation time (before network cutoff). **Note that this will
require its own separate effort to enable**.

### ❌ Credentials Get: usability issues
*Feature: kPublicKeyCredentialsGet, kOTPCredentials, kIdentityCredentialsGet*

These APIs can only be called from same-origin subframes. [An explicit
carveout](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/credentialmanagement/credential_utils.cc;l=30-35;drc=58b37f770216cc031d4e592b035134b37fe643f8)
has been made to treat all fenced frames as cross-origin and disable access to
`navigator.credentials` across the board.

### 🔺🔻🖐️ Private State/Trust Tokens: infiltration/exfiltration/fingerprinting risk
*Feature: kPrivateStateTokenIssuance, kTrustTokenRedemption*

When a private state token is issued, its existence can be observed in all
cross-origin subframes by calling `document.hasPrivateToken()`. This is a
communication channel, as an embedder can learn information about whether a
fenced frame got a trust token by calling this (and vice versa).

Each private state token can store a value between 0-5, so each token is able to
put ≈2.58 (log2(6)) bits of information into the browser that is observable from
all sites that are set up to work with the issuer.

There is a 2 token limit per site that is meant to reduce the possible data leak
(each page can potentially leak a little over 5 bits of information). However,
this also lets the fenced frame learn about its embedder by trying
`document.hasPrivateToken()` on known issuers and seeing which ones work and
which ones error out, creating a fingerprint of its context.

### 🔺✅ Attribution Reporting: partial exfiltration risk
*Feature: kAttributionReporting*

While data can be exfiltrated through reporting, as long as the reporting is
aggregate-level and not per-event, the report cannot be used to accurately
exfiltrate information or be traced back to any one frame. This API can both
send data in aggregate as well as per-event. The aggregate data is safe to send,
but the per-event data must be blocked after network revocation. The API cannot
be used to infiltrate data into the fenced frame, and no fingerprinting
identifiers are revealed by enabling this API.

Registering sources and triggers requires network connectivity. After network
revocation, the requests to the servers that establishes the sources/triggers
will not go through, so no new reports can be registered, and no sensitive
information guarded by network revocation can be added to reports.

### ✅ Cross-origin Isolated: no risk
*Feature: kCrossOriginIsolated*

This policy allows a fenced frame to use powerful features. The most concerning
feature allowed with this flag is SharedArrayBuffer, which allows different
contexts to get access to shared data. However, this is only possible if the
SharedArrayBuffer object can be passed to a different context. Since
postMessage() is already gated after network cutoff, this is okay to enable.

### 🔻🖐️ Clipboard Read: infiltration/fingerprinting risk, no exfiltration risk
*Feature: kClipboardRead*

Information set in the clipboard by an embedder can be read by a fenced frame,
allowing arbitrary data to flow into the fenced frame. This can be used to read
data directly, or for fingerprinting.

### 🔺🖐️ Clipboard Write: exfiltration/fingerprinting risk, no infiltration risk
*Feature: kClipboardWrite*

Arbitrary data can be written to the clipboard which can then be read by any
frame or any open app on the system (not just the embedding frame). This can be
used to pass data directly, or for fingerprinting.

### ✅ Web Share: no risk
*Feature: kWebShare*

While this explicitly allows data exfiltration, it does require user consent. A
popup is shown that allows the user to choose if the data is shared, and to
where it is shared. Because of that, this should be treated the same way that
having a user copy/paste a body of text is treated.

### 🖐️ Gamepad: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kGamepad*

Most features for gamepads are read-only. A notable exception is the
[`pulse()`](https://developer.mozilla.org/en-US/docs/Web/API/GamepadHapticActuator/pulse)
method. In theory, it could be used to encode data which can then be read by the
[`GamepadPose`](https://developer.mozilla.org/en-US/docs/Web/API/GamepadPose)’s
[`linearAcceleration`](https://developer.mozilla.org/en-US/docs/Web/API/GamepadPose/linearAcceleration)
property or some microphone, but the data will most likely be way too noisy to
actually be useful (it feels more like the case outlined [in this relevant
xkcd](https://xkcd.com/1172/)).

Information returned from
[`navigator.getGamepads()`](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/getGamepads)
can be used as a fingerprinting vector.

### 🔻🖐️ Screen Capture: infiltration/fingerprinting risk, no exfiltration risk
*Feature: kDisplayCapture*

Giving a fenced frame access to
[`getDisplayMedia()`](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getDisplayMedia)
will allow it to read information from other displays, which allows for data
flow into a fenced frame, not out of it. Information about the display,
including dimensions and [`the way that pages are
rendered`](https://en.wikipedia.org/wiki/Canvas_fingerprinting) can be used as a
fingerprinting vector.

### 🖐️ Shared Autofill: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kSharedAutofill*

Allowing autofill information to be shared between an embedder and its frame is
not a data exfiltration risk. Autofill information lives in the browser, and
having a subframe fill out information in a form does not reveal any information
about a fenced frame to an embedder or vice versa. This feature won’t be
particularly useful after network cutoff in unpartitioned access mode as the
autofilled form cannot be submitted anywhere.

This could potentially be a fingerprinting vector if identical forms are
submitted across fenced frame boundaries that include matching autofill data.

### 🔻🔺 Direct Sockets: infiltration/exfiltration risk post-network cutoff, no fingerprinting risk
*Feature: kDirectSockets*

This directly involves sending TCP/UDP data into and out of a fenced frame, This
is fine before network cutoff, but shouldn’t be allowed once
`window.fence.disableUntrustedNetwork()` is resolved. However, there are a few
factors at play that make disabling network for direct sockets more complicated
than other types of network access:

* It looks like direct sockets are only intended to be enabled in Isolated Web
  Apps, although other user agents could break this if they wanted to (see intro
  to
  [explainer](https://github.com/WICG/direct-sockets/blob/main/docs/explainer.md)).
* The Chrome implementation appears to back this up, by restricting usage
  heavily to "isolated apps", which is a stronger level of isolation than
  cross-origin process isolation, see function in browser-side implementation
  [here](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/direct_sockets/direct_sockets_service_impl.cc;drc=d48fd48771e97b09ab2c5497337ce0172d326792;l=235).
  However, this stricter level of isolation doesn't fully exist yet.
* Direct sockets don’t use
  [`net::IsolationInfo`](https://source.chromium.org/chromium/chromium/src/+/main:net/base/isolation_info.h),
  which is the main tool we use to disable network access.

Given the ambiguity around the status of the implementation, as well as the
network stack changes required to ensure it’s disabled properly, it makes the
most sense to be safe and disable the feature via permission in fenced frames.

### 🖐️ Window Management: fingerprinting risk, no infiltration/exfiltration risk
*Feature: kWindowManagement*

This is a fingerprinting vector. Calling
[`window.getScreenDetails()`](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetails)
allows a frame to learn information about every display connected to the
computer.

The one possibility for data exfiltration involves [calling `moveTo()` on a
popup that was opened by a fenced
frame](https://developer.chrome.com/docs/capabilities/web-apis/window-management#:~:text=it%20like%20this%3A-,popup.moveTo,-(2500%2C)).
However, since [`window.open` doesn’t give a fenced frame an
object](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt/fenced-frame/popup-noopener.https.html)
of the window that was opened, this isn’t an exfiltration concern.

### ✅ Keyboard Map: no risk
*Feature: kKeyboardMap*

This is simply used to map a keystroke to a keycode. There is no way to
exfiltrate or infiltrate information using [the API’s
methods](https://github.com/WICG/keyboard-map/blob/main/explainer.md#getlayoutmap).
Its
[`layoutchange`](https://github.com/WICG/keyboard-map/blob/main/explainer.md#layoutchange-event)
event originates from if the system changes its keyboard layout.

### 🔻🔺🖐️ Ad Auctions: infiltration/exfiltration/fingerprinting risk
*Feature: kJoinAdInterestGroup, kRunAdAuction*

Arbitrary data can be exfiltrated from a fenced frame via
`joinAdInterestGroup()` to a group-by-origin interest group. This information
can then be used in other ad auctions.

If information is leaked from elsewhere to a group-by-origin interest group
(using the above method), a fenced frame can gain access to that information by
running multiple ad auctions that are influenced by that interest group. For
each auction, the worklet will either have an auction winner or not have an
auction winner based on one of the bits stored in the interest group. Because
the fenced frame is made aware of the result of the ad auction, it can simply
store each success or failure as a bit, building up *n* bits of information by
running *n* ad auctions.

### ✅ Browsing Topics: no risk
*Feature: kBrowsingTopics, kBrowsingTopicsBackwardCompatible*

No arbitrary data can be logged as a browsing topic; there are a [fixed number
of
topics](https://privacysandbox.com/proposals/topics/#:~:text=The%20updated%20list%20contains%20around%20469%20topics)
that can be joined. The browser also [has the final
say](https://privacysandbox.com/proposals/topics/#:~:text=For%20example%2C%20the%20browser%20would%20match%20a%20sports%20website%20with%20the%20topic%20%22Sports%22.)
on what topics are shown to a context, which prevents arbitrary data from being
exfiltrated out of a frame. The fixed number of topics removes any potential
fingerprinting vectors.

### 🖐️ Local Fonts: fingerprinting risk, no exfiltration risk
*Feature: kLocalFonts*

There is no way to send data out of a fenced frame using this method. This just
involves getting information about installed fonts from the operating system.
This can be used for fingerprinting since installed fonts are unique to each
computer.

### 🔺🖐️ Shared Storage selectURL(): exfiltration/fingerprinting risk, no infiltration risk
*Feature: kSharedStorage, kSharedStorageSelectUrl*

There is no infiltration risk, as enabling this feature inside a fenced frame
will have no effect on what information about the embedder is available to the
frame.

There is no exfiltration or fingerprinting risk directly to the embedder with
this API, as it is set up (both through worklets and rendering selectURL result
in a fenced frame) to prevent the sensitive data it keeps from being exfiltrated
and joined with cross-site data. However, there is the possibility of leaking 3
bits per `selectURL()` invocation. One of 8 URLs that can be input to
`selectURL()` gets selected based on cross-site data, and the existence of the
resulting frame and the URL that was picked can be leaked via network requests
from the result rendering fenced frame. This is an ongoing privacy consideration
for fenced frames.

### ✅ Shared Storage get(): no risk
*Feature: TBD unpartitioned access feature*

This must be allowed in fenced frames. This is the way that unpartitioned data
will be read by a fenced frame after network revocation. There is an [output
gate](https://github.com/WICG/shared-storage/blob/main/README.md#output-gates-and-privacy)
specifically for access from within a fenced frame, so this data will not be
able to be exfiltrated to first-party contexts outside of the fenced frame.

### 🔻🖐️❌ Unload: infiltration/fingerprinting risk, usability issues
*Feature: kUnload*

Unload and beforeunload handlers have been [previously disallowed inside fenced
frames](https://github.com/WICG/fenced-frame/blob/master/explainer/integration_with_web_platform.md#unload-and-beforeunload-handlers)
due to issues with reliability as well as a minor communication channel concern,
so their permissions policy must be disallowed as well. The page deletion
timestamp can be used as a fingerprinting vector, or its timing can be
manipulated to send information.

### 🔻🖐️ Compute Pressure: infiltration/fingerprinting risk, no exfiltration risk
*Feature: kComputePressure*

This API can just read the CPU load state. There is no way to use it to
exfiltrate information.

In theory, an embedder or fenced frame could switch between running
computationally expensive operations and not running them in order to encode
bits to exfiltrate, and then some other frame can measure the differences in CPU
load state to decode the message.

Having the embedder and fenced frame check computation pressure states over time
can build a fingerprint of the system.

### ✅ Private Aggregation: no risk
*Feature: kPrivateAggregation*

While data can be exfiltrated through reporting, the data is aggregated, so it
can’t be traced back to any one frame or reliably joined to build a profile of
the user. No data can be infiltrated into the fenced frame using this API, nor
can this API be used as a fingerprinting vector.
