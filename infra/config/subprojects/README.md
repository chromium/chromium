Definitions of LUCI entities for the chromium project.

The following subprojects exist:

* chromium - Builders that test the chromium/src codebase.
* codesearch - Builders that test codesearch with chromium.
* findit - Builders that are used by the Sheriff-o-Matic findit service.
* reclient - Builders that test the use of reclient by chromium builders.
* webrtc - Builders that test the integration of WebRTC with chromium.

Each subproject contains a subproject.star that is the entry point for its
configuration, which execs the starlark files that define the LUCI entities for
the subproject.
