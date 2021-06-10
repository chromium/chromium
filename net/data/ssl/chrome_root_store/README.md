# Chrome Root Store

This directory contains the in development definition of the
[Chrome Root Store](https://www.chromium.org/Home/chromium-security/root-ca-policy).
It is currently not used for trust decisions in Chrome.

The root store is defined by `root_store.textproto` file, which is a
`RootStore` [protobuf](https://developers.google.com/protocol-buffers) message,
defined in
[`//net/tools/root_store_tool/root_store.proto`](/net/tools/root_store_tool/root_store.proto).
It references certificates in the `certs` directory. The
[`root_store_tool`](/net/tools/root_store_tool/root_store_tool.cc) will
files in this directory to eventually use this data for trust decisions in
Chrome.

TODO(https://crbug.com/1216547): update document when work on Chrome Root Store
is complete.
