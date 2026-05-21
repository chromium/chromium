# components/webauthn/android

This directory contains the Java orchestration and JNI translation layer
between Blink's multi-platform Web Authentication/FIDO interface
(`blink::mojom::Authenticator`) and native Android authentication backends.

## Overview

The primary role of this component is to bridge the C++ Mojo IPC boundary of the
renderer/Blink engine with native Android services and system APIs. It
translates Blink Mojo structures into Android system/platform API structures
(via Java orchestration and JNI translation) and dispatches these requests to
the appropriate native Android service.

### Request Routing & Entry Points

```
[ Standard WebAuthn Flow ]           [ Internal Flow (e.g. SPC) ]
     Blink (C++)                            Browser (C++)
          │                                       │
          │ Mojo IPC                              ▼
          │                         InternalAuthenticatorAndroid (C++)
          │                                       │
          │                                       │ JNI
          ▼                                       ▼
   AuthenticatorImpl (Java) ◄────────────── InternalAuthenticator (Java)
          │
          ▼
Fido2CredentialRequest (Java) ◄────── JNI ──────► WebauthnBrowserBridge
          │                                                   │
          │                                                   ▼
          │                                      Chrome UI controller (C++)
          │
      ┌───┴─────────────────────┬─────────────────────────┐
      │ (Android 14+)           │ (Cond. Create)          │ (Pre-A14/Fallback)
      ▼                         ▼                         ▼
CredManHelper       IdentityCredentialsHelper     Fido2ApiCallHelper
   (Java)                      (Java)                    (Java)
      │                         │                         │
      ▼                         ▼                         ▼
Credential                GMS Identity              GMS Core FIDO2
Manager (OS)              Credentials
```

---

## Underlying Backends

Depending on the host device capabilities, Android OS version, and application
configuration, requests are routed to one of three underlying system/platform
APIs:

1. **Android 14+ Credential Manager APIs**
   Conditionally invoked on devices running Android 14+ (API level 34+). It maps
   requests using `CreateCredentialRequest`, `GetCredentialRequest`, and
   `PendingGetCredentialRequest` (prefetching)
2. **GMS Core FIDO2 APIs**:
   The component bypasses the standard Play Services SDK client libraries and
   instead performs manual serialization (via `Fido2ApiCall` and
   `Fido2ApiCallHelper`) to communicate directly with GMS Core FIDO2 APIs over
   Binder IPC. This handles enumeration of credentials, as well as local
   non-discoverable credentials.
3. **GMS Core Identity Credentials APIs**:
   Communicates with `IdentityCredentialManager` to obtain an
   `IdentityCredentialClient`. Invokes `createCredential` for conditional
   credential registration flows and `signalCredentialState` to report
   credential status back to relying parties.

---

## Support Matrix

The matrices below are for non-payment requests.

### Get Calls

| Mode       | CredManSupport | Routing | Behavior                             |
| :--------- | :------------- | :------ | :----------------------------------- |
| NONE       | ANY            | -       | Request is aborted immediately. No   |
:            :                :         : native calls are dispatched.         :
| APP        | DISABLED       | FIDO    | Origin validation is omitted at the  |
:            :                :         : browser layer. Routed to GMS Core    :
:            :                :         : FIDO2 App APIs, which defer package  :
:            :                :         : signature and origin verification    :
:            :                :         : to the platform.                     :
| APP        | FULL           | CredMan | Origin validation is omitted at the  |
:            :                :         : browser layer. Invokes Android 14+   :
:            :                :         : Credential Manager APIs. It defers   :
:            :                :         : package signature and origin.        :
:            :                :         : verification.                        :
| BROWSER    | DISABLED       | FIDO    | WebView browser on pre Android 14.   |
:            :                :         : Handles the request using GMS Core   :
:            :                :         : FIDO2 Browser APIs.                  :
| BROWSER    | FULL           | CredMan | WebView browser on Android 14+.      |
:            :                :         : Handles the request using Credential :
:            :                :         : Manager                              :
| CHROME     | DISABLED       | FIDO    | Pre-Android 14 Chrome. Directly      |
:            :                :         : targets GMS Core FIDO2 Browser       :
:            :                :         : APIs, handling credential pickers    :
:            :                :         : and hybrid/cross-device flows via    :
:            :                :         : the Chrome UI.                       :
| CHROME     | FULL           | CredMan | Default Chrome routing on Android    |
:            :                :         : 14+  Offloads user                   :
:            :                :         : authentication fully to the          :
:            :                :         : Credential Manager.                  :
| CHROME     | PARALLEL       | BOTH    | Advanced Chrome integration on       |
:            :                :         : Android 14+. Queries GMS Core        :
:            :                :         : FIDO2 APIs in parallel with          :
:            :                :         : starting a prefetch request via      :
:            :                :         : Credential Manager to populate a     :
:            :                :         : unified custom Chrome/hybrid UI      :
:            :                :         : picker with local entries and a      |
:            :                :         : CredMan entry point.                 |
| CHROME_3PP | DISABLED       | FIDO    | Pre-Android 14 Chrome 3PP. Uses      |
:            :                :         : GMS Core FIDO2 Browser APIs.         :
:            :                :         : Conditional UI is disabled.          :
| CHROME_3PP | FULL           | CredMan | Chrome 3PP on Android 14+.           |
:            :                :         : Forces exclusive routing to OS       :
:            :                :         : Credential Manager.                  :

---

### Create Calls

| Mode       | CredManSupport | Routing | Behavior                             |
| :--------- | :------------- | :------ | :----------------------------------- |
| NONE       | ANY            | -       | Request is aborted immediately. No   |
:            :                :         : native calls are dispatched.         :
| APP        | DISABLED       | FIDO    | Origin validation is omitted. Uses   |
:            :                :         : GMS Core FIDO2 App APIs for key      :
:            :                :         : registration.                        :
| APP        | FULL           | CredMan | Origin validation is omitted.        |
:            :                :         : Dispatches registration directly     :
:            :                :         : to Android 14+ Credential Manager.   :
| BROWSER    | DISABLED       | FIDO    | WebView browser pre-Android 14.      |
:            :                :         : Request is routed to GMS Core FIDO2  :
:            :                :         : Browser APIs.                        :
| BROWSER    | FULL           | CredMan | WebView browser on Android 14+.      |
:            :                :         : Dispatches the request               :
:            :                :         : directly to Credential Manager       :
:            :                :         : with browser origin parameters.      :
| CHROME     | DISABLED       | FIDO    | Pre-Android 14 Chrome. Standard      |
:            :                :         : registration via GMS Core FIDO2      :
:            :                :         : Browser APIs, triggering GMS Core's  :
:            :                :         : native passkey creation prompts.     :
| CHROME     | FULL           | CredMan | Android 14+ Chrome. If `residentKey` |
:            :                :         : is required or preferred, it routes  :
:            :                :         : to Credential Manager with GPM       :
:            :                :         : display branding. If `residentKey`   :
:            :                :         : is discouraged (local-only keys)     :
:            :                :         : or for SPC payments, it falls back   :
:            :                :         : to GMS Core FIDO2 Browser APIs.      :
| CHROME     | PARALLEL       | FIDO    | Parallel registration is             |
:            :                :         : unsupported on the platform. When    :
:            :                :         : parallel support level is            :
:            :                :         : evaluated, creation requests fall    :
:            :                :         : back entirely to standard GMS Core   :
:            :                :         : FIDO2 APIs.                          :
| CHROME_3PP | DISABLED       | FIDO    | Pre-Android 14 Chrome 3PP. Standard  |
:            :                :         : registration via GMS Core FIDO2      :
:            :                :         : Browser APIs.                        :
| CHROME_3PP | FULL           | CredMan | Chrome with 3rd Party Password       |
:            :                :         : Manager on Android 14+. Offloads     :
:            :                :         : credential creation exclusively to   :
:            :                :         : Credential Manager                   :

### Legend for Support Matrices

#### Mode (WebauthnMode)

Defines the client persona and context (governing origin requirements and
custom browser-process decorations):
* **NONE**: WebAuthn completely disabled.
* **APP**: Request from custom Android application embedding WebView.
* **BROWSER**: WebView standard browser mode.
* **CHROME**: Default Chrome implementation.
* **CHROME_3PP**: Chrome with 3rd Party Autofill / Password Manager enabled.

#### CredManSupport

Evaluates and represents the host device's API capability level (governing
routing and standard GMS Core/OS integration boundaries):
* **ANY**: Any support level.
* **DISABLED**: Credential Manager is not used.
* **FULL**: Credential Manager is fully supported unless inapplicable.
* **PARALLEL**: Parallel execution support enabled (GMS Core +
  Credential Manager).

#### Routing (Barrier.Mode)

* **FIDO**: `ONLY_FIDO_2_API` — Routes to GMS Core FIDO2 APIs.
* **CredMan**: `ONLY_CRED_MAN` — Routes to Android 14+ Credential Manager APIs.
* **BOTH**: `BOTH` — Queries GMS Core in parallel with prefetching
  Credential Manager.

---

## Integration with the browser / app

`WebauthnBrowserBridge` is a specialized, embedder-specific JNI bridge
designed exclusively for **Chrome** (`CHROME` and `CHROME_3PP` modes).
It is not used in Android WebView.

Chrome provides a deeply integrated and custom browser UI for credential
selection (such as the keyboard accessory and touch-to-fill bottom sheets).

Since the underlying FIDO2/CredMan backends are accessed via the Java layer,
but the browser UI and autofill systems are orchestrated in C++ Chrome layers,
`WebauthnBrowserBridge` acts as the bidirectional JNI bridge.

---

## Password Retrieval Support

While primarily a WebAuthn component, this orchestration layer also supports
retrieving password credentials (usernames and passwords). Depending on the
CredManSupport value, passwords will be requested either:

- from password manager component or
- from Google Password Manager through Credential Manager APIs.

Passwords are requested under two main cases:

1. **uiMode: "immediate":** Triggered during standard WebAuthn Get assertions
   when `uiMode: "immediate"` is specified. It passwords can be supported
   combined with passkeys when both `publicKey` and `password` are present in
   the request, or alone.
2. **mediation: "conditional":** During conditional UI (Autofill) in
   **full CredMan mode**, passwords will be retrieved dynamically when the user
   interacts with a focused input field.
