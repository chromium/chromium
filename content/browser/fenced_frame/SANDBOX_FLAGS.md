# Fenced frame: Sandbox Flags

## How to use this document

A fenced frame is an embedded context that enforces strict boundaries between
itself and its embedder. Fenced frames are not allowed to communicate
information to and from their embedders (i.e. using something like
`window.postMessage()`), nor are they allowed to learn information about their
context that can help them form a fingerprint of where they're embedded.

This document serves as an audit for every sandbox flag to determine whether
it's safe to enable inside of a fenced frame. To determine that, we need to
answer the following questions:

1. Will unsandboxing this flag allow an embedder to send information into a
   fenced frame? For example, can the embedder modify some global state that a
   fenced frame can later observe? We refer to this in the audit as an
   "infiltration risk".
1. Will unsandboxing this flag allow a fenced frame to send information back to
   its embedder? We refer to this as an "exfiltration risk".
1. Will unsandboxing this flag allow a fenced frame to learn about the context
   it's embedded in and create a fingerprint? We refer to this as a
   "fingerprinting risk". When referring to fingerprinting in this document, we
   are referring to [active
   fingerprinting](https://www.w3.org/TR/fingerprinting-guidance/#active-0)
   specifically, as all of these require the use of JavaScript to read the
   relevant properties the APIs expose.

**If you're adding a new sandbox flag, please do the following:**

1. Answer the 3 questions above to determine the infiltration, exfiltration, and
   fingerprinting risks for your flag, if any exist.
1. Add a new entry in this document with your sandbox flag, what risks they
   carry, and a brief justification either outlining the risks or explaining why
   the API carries no risk.

Please feel free to reach out to
`third_party/blink/renderer/core/html/fenced_frame/OWNERS` with any questions
you have.

## Sandbox flags for fenced frames created with Protected Audience

Protected Audience-created fenced frames have the privacy guarantee that no
information can flow from the embedder into the fenced frame. If we allowed
arbitrary sandboxing flag sets to be applied, an embedder can enable and disable
a specific combination of flags, which the fenced frame can then test for to
create a fingerprint of the context it's in, allowing up to 1 bit per flag (up
to 15 bits, as of January 2025) to be infiltrated into the fenced frame.

To mitigate this risk, we require a specific set of sandbox flags to be
unsandboxed in the context the fenced frame loads in. If any of those flags are
sandboxed (i.e. restricted), the fenced frame will not load. The rest of the
flags are forced to be sandboxed once the fenced frame loads, even if the fenced
frame attempts to enable the flag.

See: `kFencedFrameForcedSandboxFlags` and
`kFencedFrameMandatoryUnsandboxedFlags` in
`third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h`. The forced
sandboxed and mandatory unsandboxed flags are the current behavior for all
fenced frames variants.

## Sandbox flags for fenced frames created with selectURL()

selectURL-created fenced frames can contain information from the embedder by
having the embedder add arbitrary data to the URLs that the frame is navigated
to. Because of this, it is acceptable to have information flow in from the
embedder to the fenced frame via the combination of sandboxing flags in place.

However, stopping data outflow from the fenced frame to the embedder is still
part of the privacy story. If unsandboxing a flag results in access to an API
that can exfiltrate data across a fenced frame boundary, that flag must always
be forced to be sandboxed.

## Sandbox flags for fenced frames with unpartitioned data access

When fenced frames get access to unpartitioned data, they also lose network
access. This is done to prevent the unpartitioned data from being exfiltrated
and joined with other cross-site signals. After this point, the unpartitioned
data must not be able to be sent out of the fenced frame. Otherwise, a frame
that still has network access can act as a proxy to send the data to an external
server. Note that these fenced frames can be created with the
`FencedFrameConfig(url)` constructor, which allows arbitrary data to flow in via
the URL.

## Sandbox flags audit

Below is an audit of all sandbox flags (as of January 2025) and whether enabling
them for fenced frames (that can allow embedder information to be transferred to
the FF, i.e. `FencedFrameConfig(url)` and `selectURL()`-created frames), if
needed in the future, is safe or will pose a risk:

Legend:

* ✅: No risk/issues
* 🔻: Infiltration risk
* 🔺: Exfiltration risk
* 🖐️: Fingerprinting risk
* ❌: Usability issues
* 🤬: UX concerns

Note that since Protected Audience-created fenced frames are used specifically
for ads, much of the UX concerns listed in this document are from an ads
perspective.

### 🤬 Downloads: UX concerns
*Sandbox flag: allow-downloads*

All downloads are explicitly disabled in fenced frames regardless of sandbox
flags. An ad that automatically starts a download is something we want to avoid,
since that can be used for abuse vectors. Allowing this flag will have no effect
inside of a fenced frame.

### ✅ Forms: no risk
*Sandbox flag: allow-forms*

Forms allow data to be POSTed to external servers. However, the embedding
context has no knowledge of this happening. Since forms are entirely
self-contained within the fenced frame, no new information can be infiltrated
into the fenced frame, and the form can't be used to learn about the embedding
context.

### 🤬 Modals: UX concerns
*Sandbox flag: allow-modals*

Modals (such as `alert()`, `confirm()`, and `print()`) are explicitly disallowed
in fenced frames. Allowing an ad to open an alert box will be extremely
irritating to the user, regardless of whether the user gave the frame user
activation. This flag will and should have no effect inside of a fenced frame.

### 🤬 Orientation Lock: UX concerns
*Sandbox flag: allow-orientation-lock*

Orientation lock is
[deprecated](https://developer.mozilla.org/en-US/docs/Web/API/Screen/lockOrientation)
because allowing subframes to lock the orientation of an entire device's screen
rotation can easily be confusing and frustrating for users. The replacement,
`orientation.lock()`, is also explicitly disallowed in fenced frames for that
same reason. This flag will and should have no effect inside of a fenced frame.

### 🤬 Pointer Lock: UX concerns
*Sandbox flag: allow-pointer-lock*

An ad locking the user's pointer is very bad UX, so pointer lock is always
disabled in fenced frames regardless of the sandbox flag.

### ✅ Pop-ups: no risk
*Sandbox flag: allow-popups*

Data is sent to external servers in order to open a pop-up window. However, the
embedding context has no knowledge of this happening, and servers can only be
provided information that already exists in the fenced frame. Once a pop-up is
created, it will follow a no-opener relationship with the fenced frame and not
be able to send any data back to it. Any potential risks with unpartition data
access are mitigated simply by the network revocation mechanism preventing any
network requests from going out after the fenced frame gets access.

### ✅ Pop-ups escape sandbox: no risk
*Sandbox flag: allow-popups-to-escape-sandbox*

This is currently forced to be unsandboxed. If we allow this to be sandboxed,
then a pop-up could inherit the sandboxing flag set of the fenced frame, using
that to build a fingerprint of the context that opened it. However, the same
amount of information can be conveyed simply by passing in query parameters to
the URL. For that reason, there is no additional risk allowing this restriction
introduces.

### 🔻🔺 Presentations: infiltration/exfiltration risk
*Sandbox flag: allow-presentation*

Information about [display availability for
presentations](https://developer.mozilla.org/en-US/docs/Web/API/PresentationAvailability)
is available across contexts. If one context enters presentation mode, that can
be observed by another context. Therefore, this must not be allowed inside of
fenced frames.

### ✅ Same-origin: no risk
*Sandbox flag: allow-same-origin*

This flag is currently forced to be unsandboxed. The fenced frame getting
information about its origin does not pose any risk since fenced frames cannot
access origin scoped storage. Their storage, cookies, and network access is
further guaranteed to be ephemeral via a nonce in the corresponding storage,
cookie, and network isolation keys as described in [this
explainer](https://github.com/WICG/fenced-frame/blob/master/explainer/storage_cookies_network_state.md).

### ✅ Scripts: no risk
*Sandbox flag: allow-scripts*

While there exist APIs that are called via JavaScript that are able to leak
information across fenced frame boundaries, disabling all scripts for all fenced
frames would be too drastic of a move and would render fenced frames useless.
Instead, our approach is to tackle the offending APIs directly, disallowing them
within fenced frames rather than all script calls, problematic or otherwise.

### 🔻🔺🖐️ Storage Access With User Activation: infiltration/exfiltration/fingerprinting risk
*Sandbox flag: allow-storage-access-by-user-activation*

The [Storage Access
API](https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API) allows
subframes to get access to unpartitioned third party cookies, an explicit
non-goal of fenced frames. Allowing this will open an unrestricted communication
channel across fenced frame boundaries.

### ✅ Top-level Navigation With User Activation: minimal risk
*Sandbox flag: allow-top-navigation-by-user-activation*

For most use cases, no information can be exfiltrated that can't already be
exfiltrated via a `fetch()` request.

Fenced frames created with
[`selectURL()`](https://github.com/WICG/shared-storage?tab=readme-ov-file#select-url)
can have up to 3 bits of cross-site data per navigation (since `selectURL()`
takes up to 8 URLs as input) which can be leaked as part of a top-level
navigation. However, there are mitigations in place in the Shared Storage API to
minimize this risk and make this acceptable to enable in fenced frames.

See: https://github.com/WICG/shared-storage/blob/main/select-url.md#privacy

### 🖐️🤬 Top-level Navigation Without User Activation: fingerprinting risk, UX concerns
*Sandbox flag: allow-top-navigation*

This has the same risks as `allow-top-navigation-by-user-activation`, but now
there does not need to be a user gesture to send the information out. Because
this can now happen automatically, this is more of a risk for
`selectURL()`-generated fenced frames.

Allowing fenced frames to open pop-ups without ever getting any user interaction
can result in a bad user experience. This should not be allowed. even in cases
where no information can be leaked.

### ✅ Top-level Navigation to Custom Protocols: no risk
*Sandbox flag: allow-top-navigation-to-custom-protocols*

A fenced frame making a request to an http:// or https:// endpoint is no
different from a privacy standpoint than a request to an ftp:// or a mailto://
endpoint. Information can be encoded in each protocol's request, and none of
them give the fenced frame access to any new information about its context or
vice versa.

### ❌ SameSite=None Cookies: usability issues
*Sandbox flag: allow-same-site-none-cookies*

Fenced frames [only allow access to partitioned
cookies](https://github.com/WICG/fenced-frame/blob/master/explainer/storage_cookies_network_state.md#cookies).
Allowing `SameSite=None` cookies allows cookies to be shared between sites and
cross-site requests, but that is only useful in contexts where this kind of
unpartitioned cookie access is allowed to begin with. Since fenced frames force
all cookies to be partitioned regardless, enabling this flag inside a fenced
frame will be a no-op.
