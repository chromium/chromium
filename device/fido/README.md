# Security Keys

Security keys are physical devices that often connect via USB and have a button. They can generate public keys and sign with them to authenticate a user and are most often used as a second factor for security.

Websites interact with them via two APIs: the older [U2F API](https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-javascript-api-v1.2-ps-20170411.html) and the modern [W3C Webauthn API](https://www.w3.org/TR/webauthn/). In Chromium, the U2F API is not directly supported but it can be used by using `postMessage` with an internal extension called [cryptotoken](/chrome/browser/resources/cryptotoken/). Webauthn is supported by Blink and is part of [CredMan](https://www.w3.org/TR/credential-management-1/).

(Historically cryptotoken contained a complete stack that interacted with USB devices directly. Now, however, it's a wrapper layer over the Webauthn APIs.)

Several different types of security keys are supported. Older security keys implement the [U2F protocol](https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html) while more modern ones implement [CTAP2](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html). These devices can work over USB, Bluetooth Low Energy (BLE), or NFC (not supported). Additionally Chromium contains support for using Touch ID on macOS as a security key as well support for forwarding requests to the native libraries on modern versions of Windows.

## Life of a request

This section provides a coarse roadmap for understanding the code involved in security key support by highlighting the path that a login request might take.

Firstly, the CredMan `get` call ends up in [`CredentialsContainer::get`](https://cs.chromium.org/search/?q=symbol:CredentialsContainer::get+exact:yes&det=matsel&sq=package:chromium&type=cs). CredMan supports several types of credentials but the code dealing with `publicKey` relates to security key support.

The request is packaged into a Mojo call defined in [authenticator.mojom](/third_party/blink/public/mojom/webauthn/authenticator.mojom). On Android, that Mojo request is handled by Android-specific code and is forwarded to support libraries in Google Play Services. Otherwise the Mojo interface will be bound to [`AuthenticatorCommon`](/content/browser/webauth/authenticator_common.cc); specifically it'll call [`AuthenticatorCommon::GetAssertion`](https://cs.chromium.org/search/?q=symbol:AuthenticatorCommon::GetAssertion+exact:yes&sq=package:chromium).

AuthenticatorCommon is part of Chromium's [content layer](https://www.chromium.org/developers/content-module) and so [calls into the embedder](https://cs.chromium.org/search/?q=symbol:GetWebAuthenticationRequestDelegate+exact:yes&sq=package:chromium) to get a [delegate object](https://cs.chromium.org/search/?q=symbol:AuthenticatorRequestClientDelegate+exact:yes) that allows it to perform actions like showing UI. It also triggers the lower-level code to start the process of finding an authenticator to handle the request. For an assertion request it'll create a [`GetAssertionRequestHandler`](https://cs.chromium.org/search/?q=symbol:GetAssertionRequestHandler+exact:yes) from this directory.

The `Handler` classes manage a specific user action and their first job is to [initiate discovery](https://cs.chromium.org/search/?q=symbol:FidoRequestHandlerBase::InitDiscoveries+exact:yes) of possible security keys. The discovery process will find candidate USB, BLE, Touch ID, etc devices, each of which will be fed into [`DispatchRequest`](https://cs.chromium.org/search/?q=symbol:GetAssertionRequestHandler::DispatchRequest+exact:yes). Different actions may be taken depending on features of the discovered authenticator. For example, an authenticator which cannot handle the request may be asked to wait for a touch so that the user can still select it, even though it'll cause the request to fail. These per-authenticator operations will be dispatched via the abstract [`FidoAuthenticator`](https://cs.chromium.org/search/?q=symbol:FidoAuthenticator+exact:yes) interface.

If a per-authenticator operation is complex and requires several steps it will be handled by a &ldquo;task&rdquo;. In this example, a [`GetAssertionTask`](https://cs.chromium.org/search/?q=symbol:device::GetAssertionTask+exact:yes) will likely be created by a [`FidoDeviceAuthenticator`](https://cs.chromium.org/search/?q=symbol:device::FidoDeviceAuthenticator+exact:yes), the implementation of `FidoAuthenticator` used by physical devices.

The assertion task knows how to sequence a series of U2F or CTAP2 operations to implement an assertion request. In the case of U2F, there will be another layer of state machines in, e.g., [`U2fSignOperation`](https://cs.chromium.org/search/?q=symbol:device::U2FSignOperation+exact:yes) because U2F has a historical authenticator model.

If interaction with UI is required, for example to prompt for a PIN, the handler will make calls via the [`Observer`](https://cs.chromium.org/search/?q=symbol:device::FidoRequestHandlerBase::Observer+exact:yes) interface, which is implemented by the embedder's UI objects that were created by `AuthenticatorCommon`.

## Settings

It's also possible for security key operations to be triggered by actions in the Settings UI: there are several security key actions that can be taken on `chrome://settings/securityKeys`. In this case, calls from the Javascript that implements the Settings UI end up in [`SecurityKeysHandler`](https://cs.chromium.org/search/?q=symbol:settings::SecurityKeysHandler+exact:yes), which then operates in the same way as `AuthenticatorCommon`, albeit without creating any native UI.

## Fuzzers

[libFuzzer] tests are in `*_fuzzer.cc` files. They test for bad input from
devices, e.g. when parsing responses to register or sign operations.

[libFuzzer]: /testing/libfuzzer/README.md
