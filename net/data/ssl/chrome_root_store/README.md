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

To test the Chrome Root Store, do the following:

### On Windows (M102 or higher)
1. Enable the Chrome Root Store by starting Chrome with the following flag: 
`--enable-features=ChromeRootStoreUsed`
  
2. Navigate to https://rootcertificateprograms.edicom.es/ (trusted by Windows, but not the Chrome 
Root Store)
     - **Expected outcome with Chrome Root Store enabled:** Page does not load 
     (NET::ERR_CERT_AUTHORITY_INVALID)
     - **Expected outcome with Chrome Root Store disabled:** Page loads

### On macOS (M105.0.5122.0 or higher)
1. Enable the Chrome Root Store by starting Chrome with the following flags: 
`--enable-features=ChromeRootStoreUsed,CertVerifierBuiltin:impl/4`
2. Navigate to https://valid-ctrca.certificates.certum.pl/ (not trusted by macOS, but trusted by the
Chrome Root Store)
     - **Expected outcome with Chrome Root Store enabled:** Page does not load 
     (NET::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED)
     - **Expected outcome with Chrome Root Store disabled:** Page does not load 
     (NET::ERR_CERT_AUTHORITY_INVALID)

## Determine which version of the Chrome Root Store is in use (M104.0.5110.0 or higher)
1. Collect and export a [NetLog
dump](https://www.chromium.org/for-testers/providing-network-details/) (open a new tab and connect 
to at least one website during log generation)
2. Import the log using [NetLog Viewer](https://netlog-viewer.appspot.com/#import)
3. Navigate to "Events" (left-hand navigation)
4. Search for events that include `CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION`. The in-use version 
of the Chrome Root Store is printed after `CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION` as it appears
in a specific event entry.

**Note:** If you do not observe any events that include `CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION`
, the Chrome Root Store is **not** in use.

## I've enabled the Chrome Root Store on my platform, how can I tell which certificates are trusted? (M105.0.5122.0 or higher)
- The current contents of the Chrome Root Store is available
[here](/net/data/ssl/chrome_root_store/root_store.md)
- The Chrome Root Store is updated by [Component Updater](/components/component_updater/README.md). 
To observe the contents of the Chrome Root Store in use by your version of Chrome:
     1. Navigate to `chrome://system`
     2. Click the `Expand...` button next to `chrome_root_store`
     
  
