# Testing the Chrome Root Store and Certificate Verifier
Last updated: September 1, 2022

To test the Chrome Root Store & Certificate Verifier, do the following:

## On Windows (M102 or higher)
1. Enable the Chrome Root Store & Certificate Verifier by starting Chrome with
the following flag: <br>`--enable-features=ChromeRootStoreUsed`

2. Navigate to https://rootcertificateprograms.edicom.es/ (trusted by Windows,
but not the Chrome Root Store)
     - **Expected outcome with Chrome Root Store enabled:** Page does not load
     (NET::ERR_CERT_AUTHORITY_INVALID)
     - **Expected outcome with Chrome Root Store disabled:** Page loads

## On macOS (M105.0.5122.0 or higher)
1. Enable the Chrome Root Store & Certificate Verifier by starting Chrome with
the following flags: <br>`--enable-features=ChromeRootStoreUsed`

2. Navigate to https://valid-ctrca.certificates.certum.pl/ (not trusted by
macOS, but trusted by the Chrome Root Store)
     - **Expected outcome with Chrome Root Store enabled:** Page loads
     - **Expected outcome with Chrome Root Store disabled:** Page does not load
     (NET::ERR_CERT_AUTHORITY_INVALID)

## On Android (M114.0.5733.0 or higher)

1. Enable the Chrome Root Store and Certificate Verifier by going to
   chrome://flags, searching for the flag "Chrome Root Store", and set it to
   enabled. This will then require Chrome to restart.

2. Navigate to https://valid-ctrca.certificates.certum.pl/ (not trusted by
Android 13, but trusted by the Chrome Root Store)
     - **Expected outcome with Chrome Root Store enabled:** Page loads
     - **Expected outcome with Chrome Root Store disabled:** Page does not load
     (NET::ERR_CERT_AUTHORITY_INVALID)
