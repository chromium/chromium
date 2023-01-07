# Advice to sites regarding `PublicKeyCredentialCreationOptions.pubKeyCredParams`

In the options for a [Web Authentication](https://www.w3.org/TR/webauthn/) [credential registration request](https://www.w3.org/TR/webauthn/#createCredential), the caller can specify a list of [cryptographic algorithm identifiers](https://www.w3.org/TR/webauthn-2/#typedefdef-cosealgorithmidentifier) in the [`pubKeyCredParams`](https://www.w3.org/TR/webauthn-2/#dictdef-publickeycredentialparameters) field.

If left unspecified, Chrome uses the default values of `ES256` (-7) and `RS256` (-257).

In some situations, a [Relying Party](https://www.w3.org/TR/webauthn-2/#webauthn-relying-party) developer might choose to augment this list with other identifiers. However, developers should be aware that excluding either of the default identifiers has compatibility risks. In particular, `RS256` is necessary for compatibility with Microsoft Windows platform authenticators. `ES256` is a widely supported algorithm and is compatible with most other platform authenticators and roaming authenticators.

Therefore a Relying Party that uses an algorithm identifier list that omits either of those values will see registration failures when users attempt to use incompatible authenticators.
