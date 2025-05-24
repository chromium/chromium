# crypto/obsolete

This directory contains implementations of obsolete cryptographic primitives
that we still need for compatibility with deployed protocols or file formats.
All new uses of this code require approval by a `//CRYPTO_OWNERS` member. This
requirement is enforced by requiring client code to be added to a friend list
for the specific obsolete primitive that the client code wants to use, which
has to be done by a crypto owner.

If you are designing a new protocol, or writing a new implementation, and
believe you need to use one of these primitives, please contact a
`//CRYPTO_OWNERS` member or security@chromium.org.
