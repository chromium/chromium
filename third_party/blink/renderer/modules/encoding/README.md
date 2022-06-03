# Encoding API

This directory contains Blink's implementation of [the Encoding
Standard API](https://encoding.spec.whatwg.org/#api).

Actual encoding and decoding is delegated to implementations of the
[WTF::TextCodec](../../platform/wtf/text/text_codec.h) interface in
[platform/wtf/text](../../platform/wtf/text). For most encodings we then
delegate to [ICU](../../../../icu). The [ISO-2022-JP
encoding](https://encoding.spec.whatwg.org/#iso-2022-jp) implementation of ICU
has been patched for better conformance with the Encoding Standard.
