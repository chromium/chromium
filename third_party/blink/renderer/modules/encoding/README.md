# Encoding API

This directory contains Blink's implementation of [the Encoding
Standard API](https://encoding.spec.whatwg.org/#api).

Actual encoding and decoding is delegated to implementations of the
WTF::TextCodec interface. For most encodings we then delegate to ICU. The
version of ICU used by Blink has been patched for better conformance with the
standard.
