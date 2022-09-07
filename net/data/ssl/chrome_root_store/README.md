# Chrome Root Store

This directory contains the artifacts related to the
[Chrome Root Store](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/root_store.md).

The root store is defined by two files:

* `root_store.textproto` file, which is a `RootStore`
  [protobuf](https://developers.google.com/protocol-buffers) message, defined in
  [`//net/cert/root_store.proto`](/net/cert/root_store.proto).

* `root_store.certs` which stores the certificates referenced by
  `root_store.textproto`

The [`root_store_tool`](/net/tools/root_store_tool/root_store_tool.cc) uses the
two files above to generate code that is included in Chrome. The Chrome Root
Store and Certificate Verifier will begin rolling out on Windows and macOS in
Chrome 105, with other platforms to follow.

## Additional Information
Learn more about testing with the Chrome Root Store and Certificate Verifier
[here](testing.md).

Learn more about the Chrome Root Program [here](https://g.co/chrome/root-policy).

See "Frequently Asked Questions" [here](faq.md).