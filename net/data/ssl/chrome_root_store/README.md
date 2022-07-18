# Chrome Root Store

This directory contains the in development definition of the
[Chrome Root Store](https://www.chromium.org/Home/chromium-security/root-ca-policy).
It is currently not used for trust decisions in Chrome.

The root store is defined by two files:

* `root_store.textproto` file, which is a `RootStore`
  [protobuf](https://developers.google.com/protocol-buffers) message, defined in
  [`//net/cert/root_store.proto`](/net/cert/root_store.proto).

* `root_store.certs` which stores the certificates referenced by
  `root_store.textproto`

The [`root_store_tool`](/net/tools/root_store_tool/root_store_tool.cc) uses the
two files above to generate code that is included in Chrome. This generated code
will eventually be used for trust decisions in Chrome.

## Testing

To test the Chrome Root store, do the following:

* On M102 or higher on Windows, run Chrome with the following flag:

  `--enable-features=ChromeRootStoreUsed`

  As of 2022-06, an example of a web site that is trusted by Windows Root Store
  but not by Chrome Root Store is https://rootcertificateprograms.edicom.es/.
  This can be used to test if Chrome Root Store is turned on or not (for best
  results use a fresh incognito window to avoid any caching issues).

* On 105.0.5122.0 or higher on Mac, run Chrome with the following flag:

  `--enable-features=ChromeRootStoreUsed,CertVerifierBuiltin:impl/4`

If you're running 104.0.5110.0 or higher, the currently used Chrome Root Store
version can be seen in a [NetLog
dump](https://www.chromium.org/for-testers/providing-network-details/).
