# //crypto README

This directory contains implementations of crypto primitives for use in
Chromium. Most of these are either:

* Wrappers around platform-specific APIs (DPAPI, libsecret, etc), so that code
  elsewhere in Chromium can use cross-platform abstractions, or
* Wrappers around BoringSSL APIs that use Chromium-native types like base::span
  and similar

There is very little actual cryptographic code in //crypto - it is mostly
wrappers.

This directory is actively being refactored as of 2025-06. See
[PLAN.md](PLAN.md).

## Commonly-Used Interfaces

* AEAD: [crypto/aead](aead.h)
* Hashing: [crypto/hash](hash.h)
* HMACs: [crypto/hmac](hmac.h)
* Key derivation: [crypto/kdf](kdf.h)
* Public / private keys: [crypto/keypair](keypair.h)
* Randomness: [crypto/random](random.h)
* Signatures: [crypto/sign](sign.h)

Many interfaces in this directory are deprecated and being changed or removed;
check the comment at the top of the header file before using them.

## Advice For Clients

* Ciphertext, keys, certificates, and other cryptographic material are generally
  sequences of bytes, not characters, so prefer using byte-oriented types to
  represent them: `vector<uint8_t>`, `array<uint8_t>`, and `span<uint8_t>`
  rather than `string` and `string_view`.
* To serialize private keys, use `keypair::PrivateKey::ToPrivateKeyInfo()`,
  which returns a [PKCS#8][pkcs8] PrivateKeyInfo structure serialized as a
  byte vector. To unserialize keys in this format, use
  `keypair::PrivateKey::FromPrivateKeyInfo()`.
* To serialize public keys, use `keypair::PublicKey::ToSubjectPublicKeyInfo()`
  or `keypair::PrivateKey::ToSubjectPublicKeyInfo()`, which return a
  [X.509][x509] SubjectPublicKeyInfo structure serialized as a byte vector. To
  unserialize public keys in this format, use
  `keypair::PublicKey::FromPublicKeyInfo()`.
* SubjectPublicKeyInfo and PrivateKeyInfo can represent many kinds of keys, so
  code that expects a specific kind of key must check the kind after
  deserialization.
* To serialize symmetric keys (AEAD, HMAC, or symmetric encryption keys), use a
  raw sequence of bytes for the key material. Represent these keys in memory
  using `vector<uint8_t>`, `array<uint8_t>`, or `span<uint8_t>` directly.

[pkcs8]: https://datatracker.ietf.org/doc/html/rfc5208
[x509]: https://datatracker.ietf.org/doc/html/rfc5280
