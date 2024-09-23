# Clear Key CDM

This folder contains a library CDM implementation that implements the
cdm::ContentDecryptionModule interface to support **External Clear Key** key
system ("org.chromium.externalclearkey" and variants), which is a superset of
the Clear Key key system.

Clear Key CDM is developed to test the library CDM path. It implements the basic
functionality of Clear Key key system, as well as additional features like
persistent license support. It also performs various unit tests based on the
key system suffix (e.g. "org.chromium.externalclearkey.storageidtest").
