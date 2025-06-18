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
