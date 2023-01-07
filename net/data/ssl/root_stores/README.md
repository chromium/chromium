# Root Stores

This directory contains information about the contents of various platforms'
and libraries trust stores, used for assessing risk and interoperability. Only
certificates trusted for SSL/TLS issuance are included.

As updating these stores requires manual curation, and as histograms require
being checked in, the generated files are not part of the build process, but
instead updated via [update_root_stores.py](update_root_stores.py) to update
[//net/cert/root_cert_list.h](/net/cert/root_cert_list.h), and using
[//tools/metrics/histograms/update_net_trust_anchors.py](/tools/metrics/histograms/update_net_trust_anchors.py)
to update the associated histograms.

## Format

Note: All SHA-256 hashes are stored as hex-encoded strings.

``` json

{
  "certificates": {
    "sha256_of_certificate": [
      "root_store_1",
      "root_store_2",
    ],
  },
  "last_spki_id": integer_used_for_histogram_purposes,
  "spkis": {
    "sha256_of_subjectPublicKeyInfo": {
      "id": integer_used_for_histogram_purposes,
      "fingerprints": [
        "sha256_of_trust_anchor_with_this_spki",
        "sha256_of_another_trust_anchor_with_this_spki",
      ]
    }
  }
}
```

The choice of this format was to allow for the following scenarios:

* Easy generation of histogram enumerations for associated SPKIs
* The ability to prune certificates (and SPKIs) as Chromium platforms are
  deprecated.
* The ability to update the root store metadata effectively, such as via JSON
  Patch, while maintaining the stable ordering necessary for histogram code.

## Root Store Sources

### Android

Prior to Android N (Nougat), the set of trust anchors included in Android
were provided in the [platform/libcore](https://android.googlesource.com/platform/libcore)
repository, under `luni/src/main/files/cacerts`

Beginning with Android N, the set of trust anchors included in Android is
provided in the [platform/system/ca-certifcates](https://android.googlesource.com/platform/system/ca-certificates)
repository, under `files`.

### Apple macOS

The set of root certificates for macOS is available at https://opensource.apple.com/.

Since macOS 10.4 (Tiger), the set of root certificates included is available
within the `security_certificates` package, which is independently versioned
from other packages in macOS. Only revisions since 10.9 whose package contents
had changed were included for consideration.

Additional restrictions upon trusted CAs are maintained both within the code
of Security.framework and through additional plist expressions, such as for
allowlisted certificates. However, these were not consulted, as they're not
applicable to this use case.

### Mozilla NSS

Mozilla NSS independently versions the set of included root certificates from
the NSS library version. The root package is known within the source as
`nssckbi`, maintained in `lib/ckfw/builtins`. The version can be extracted
from `nssckbi.h`, while the trust store is maintained within `certdata.txt`.

Additional restrictions upon trusted CAs are maintained both within the code
of NSS and Mozilla Firefox; however, these were not consulted, as they're not
applicable to this use case.

### Microsoft Windows

Microsoft Windows maintains its root certificates in two locations - within
a resource of `crypt32.dll`, shipped with the appropriate Windows release, and
through the Automatic Root Update (AuthRoot) mechanism, served at
http://www.download.windowsupdate.com/msdownload/update/v3/static/trustedr/en/authroot.cab

The contents of the cab file are a [PKCS#7 trust store](http://unmitigatedrisk.com/?p=259),
with attribute OIDs that match to `PROP_ID` documented in `wincrypt.h` and,
less exhaustively, on [MSDN](https://msdn.microsoft.com/en-us/library/windows/desktop/aa376079(v=vs.85).aspx)

Additional restrictions upon trusted CAs are maintained as properties within
the STL; however, these were not consulted, as they're not applicable to this
use case.

Tools that can help get this data:

* https://github.com/robstradling/authroot.stl
* https://github.com/zmap/rootfetch
