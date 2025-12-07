Previous versions are described at:
 * [Version 3.1](https://chromium.googlesource.com/chromium/src.git/+/master/docs/updater/protocol_3_1.md)
 * [Version 3](https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md)
 * [Version 2](https://github.com/google/omaha/blob/master/doc/ServerProtocolV2.md)
 * Version 1 of the protocol was never publicly deployed.

[TOC]

NOTE: this is a draft version of Omaha Protocol 4.0.


## Introduction
The Omaha protocol defines the interactions between a software updater (an
"Omaha Client") and a cloud update infrastructure. The cloud infrastructure
contains both an update control server (an "Omaha Server") and a collection of
plain HTTP servers or CDNs (collectively, the "Download Servers"). The protocol
is an application-layer protocol on top of HTTP.

Omaha clients interact with the servers in a three-phase **session**. Each step
in the session involves at least one successful HTTP transaction.
 1.  **The Update Check**: The client transmits what applications it has
     installed that may be eligible for update to the server. The server replies
     with whether an update is available for each application. If no updates are
     available, this is the end of the update session.
 2.  **The Download(s)**: The client downloads the updates from the download
     servers. It then executes downloaded installers or its own code to apply
     these updates.
 3.  **The Ping-Back(s)**: The client transmits data about its activities back
     to the server, including whether the updates were successful, which
     download URLs were used, and any error codes encountered.

The update check is an HTTP POST with a JSON data body. The response has a JSON
data body. The integrity of the update check is protected by [CUP](cup.md), even
in the presence of compromised TLS.

The downloads are HTTP GET requests. The client is free to use any HTTP
technology to fetch the downloads, for example using Range requests.

The ping-back is an HTTP POST with a JSON data body. The response may contain a
JSON data body acknowledging the received data, but should be ignored by the
client unless CUP is in use on this request.

In order to maintain consistency with prior protocols, all objects are designed
to be serializable in both XML and JSON. Type names defined in this document are
organizational only, and are not actually serialized as a part of any response
or request.

For example, we define a `hash` object named `in` represented as:
> JSON:
> "in": { "sha256": "hashvalue" }
> XML:
> <in sha256="hashvalue" />

There are multiple ways to serialize lists of objects. JSON style suggests that
lists should have plural property names. In XML, we opted for repeated fields
with the same pluralized node name. This allows the JSON to be represented as a
list with the same node name represented only once. This decision also has the
added benefit that even in cases where only one element is provided in the XML,
the name indicates that this type should be interpreted as a list of size 1.

For example, we define a list of `hash` objects, named `cached_items`, which is
represented as:
> JSON:
> "cached_items": [
>   { "sha256": "hashvalue" },
>   { "sha256": "otherhashvalue" }
> ]
> XML:
> <cached_items sha256="hashvalue" />
> <cached_items sha256="otherhashvalue" />

## Concepts

### Session ID
All requests in a session are linked by a unique **session ID**. Session IDs are
random strings with at least 128 bits of entropy. There must be no relationship
between a previous session ID and the next session ID. In the update check and
ping-back requests the session id is transmitted in the POST body (details
below). In the download requests it is transmitted as an HTTP header (details
below).

### Request ID
Each update check has a unique **request ID**. Each ping-back has a different
unique request ID. Request IDs are random strings with at least 128 bits of
entropy. There must be no relationship between a previous request ID and the
next request ID. Request ID allows an Omaha server to deduplicate repeated
requests, which have been observed as part of client or proxy retry loops.

### Default Values
The request bodies of the update check and ping-back requests are JSON. Most
members have a default value defined by this protocol. Clients and servers may
omit serialization of any members the have default values. Therefore, if a
client or server does not send the member, the recipient must assume the default
value of that member.

The default value of any list-valued member is the empty list. The default value
of any object-valued member is an empty object (which is equivalent to an object
of the appropriate type populated with its default members).

Compatible clients and servers must be able to tolerate unexpected members.

### Version Numbers
Version numbers are strings of the form A.B.C.D, where A, B, C, and D are
base 10 integers between 0 and 2^32 - 1 (inclusive). Suffix elements may be
omitted if equal to 0; that is, "1.2" is equal to  "1.2.0.0".

Version numbers are ordered. Version A is greater than version B if and only if
there some element of version A is greater than the corresponding element of
version B, and all the preceding elements are equal. Two versions are equal if
any only if all their elements are equal.

### Application IDs
An application ID is a string that identifies an application, for example
"com.google.chrome". Application IDs are case-insensitive in the ASCII
characters. Application IDs should only contain printable, human-readable
characters.

### User Counting
The Omaha protocol is anonymous, but allows servers to compute the number of
unique active or update-checking devices. This is accomplished through
client-regulated counting.

The idea of client-regulated counting is to inspect all update check requests
received over the last N days and discard all but the first request from each
client. The number of remaining requests is equal to the number of unique
clients.

Each response from the server contains a numeric date, representing the date (in
the server's choice of timezone) that the client's request was received. The
client stores this date, and sends it on the next request to the server. When
inspecting the next request, the server can determine whether the date is before
(current date - N + 1). If so, this is the first request from the client in the
N-day window. Otherwise, this request is a "duplicate" and can be discarded.

In certain environments (for example, frequently re-imaged VMs in internet
cafes), it is likely that the client may fail to update the date of the last
transaction. To avoid overcounting such clients, a technique called
ping-freshness is used. A ping-freshness value is a random string with at least
128 bits of entropy that is written into the client's data store alongside the
server's date. This value is sent along with the date in the next request. If
the server receives multiple requests with the same ping-freshness, this is a
signal that a machine's state has been cloned or reset and that the dates in the
request are not trustworthy. A new ping-freshness value is written any time a
new date is written.

In addition to counting the total population for each application, the Omaha
protocol counts the active user population for each application. When an
application is actively used, it signals this to the client, and the client will
transmit the "last reported active" date to the server on the next update check
(in addition to the "last checked" date). These dates are separately writeable
and therefore need separate ping-freshness values as well.

The "last reported active" and "last checked" date may vary per managed
application, and the client must maintain separate dates (and separate
ping-freshness values) per application.

### Differential Updates
A differential update achieves better compression by relying on information
(e.g. cached payloads) that the client already has.

A version number is usually insufficient to identify the binaries the client
already has, since they may vary by architecture, platform, or other variables
while retaining the same official version number. Therefore, the client sends
hashes of the binaries it has cached.

### Pipelines
A pipeline represents a series of operations to obtain and process a payload in
order to update to a new version of a given product. The protocol defines
various operations to support full, differential, and/or compressed update
payloads. The client must specify when sending a request what pipeline
operations it supports. Additional pipeline operations may be added in the
future.

The update response sent to the client contains a list of pipelines, each
producing the same end result. The pipelines are listed in order of preference,
and must be attempted by by the client in that order. If a pipeline fails, the
client will attempt the next pipeline available, proceeding until either a
pipeline is successful, or all pipelines have failed. This increases the
probability that a pipeline will be applied, even if all differential pipelines
fail.

Some pipeline operations have an `in` member, describing the hash of the
file that the operation will operate on. If a file with a matching hash is
already cached, clients may skip the preceding operations. Similarly, clients
may skip operations preceding and including any operation that has an
`out` member if they already have a file matching `out` cached.
Since the contents of the cache may change as a client attempts and falls back
between pipelines, the server cannot always predict what operations a client
may skip.

### Extensions & Forward Compatibility
The protocol is extensible via the addition of new object members. Clients must
tolerate the existence of members they do not handle. Unofficial or
application-specific additional members should be prefixed with an underscore to
avoid colliding with later additions to the protocol.

Additions to the protocol that specify backward-compatible default values (often
with a semantic meaning of "unknown") do not need to increase the protocol
version. Removals of values with specified defaults from the protocol do not
need to increase the protocol version, since the default value can be assumed by
compatible endpoints. All other changes to the protocol may require a new
version number.

### Timing & Backoff
Clients and servers are free to negotiate the rate at which the client conducts
update sessions. However, the following best practices are recommended:
 *   If a client periodically checks according to a time-elapsed scheduled task,
     (e.g. "check every 5 hours"), those checks should happen at an interval
     that is coprime with a 24-hour day. This prevents a specific client from
     being locked to a particular set of times at which it checks for an update.
 *   As much as possible while respecting a user or administrator's preferences,
     clients should avoid check policies that rely on specific clock values
     (e.g. "check at 5:00 AM"). Clocks throughout the world are roughly
     synchronized and update schedules like this cause abrupt spikes in update
     load on update infrastructure.
 *   If a client cannot conduct a check at the scheduled time (it is in an off
     or standby power state or lacks network connectivity, the client should
     check as soon as it recovers from that condition (subject to the below).
     This allows clients who are only briefly connected to reliably update.
 *   A client doing periodic checks should implement a random chance to postpone
     a periodic update by a fraction of that period. Client update schedules can
     become synchronized by server-side or network outages, by image cloning, by
     synchronized installations, or other effects. This behavior gradually
     desynchronizes a cohort of clients that have become synchronized for any
     reason. A 20% chance to delay by 0% to 20% of the period (chosen randomly)
     is generally good enough.
 *   Clients should avoid checking at a predictable millisecond within the
     second, a predictable second within the minute, or a predictable minute
     within the hour. This both helps resist fingerprinting of the client and
     smooths load on the server, which might otherwise have spikes of traffic at
     precisely 1:00:00.000, 1:00:01.000, 1:01:00.000, etc.

In the case of update check failure, retries are generally permitted. The client
is free to attempt an immediate retry or a retry after a delay.
At scale, even a single retry may double load on the update server regardless of
any client delay (unless the client simply delays to its next normal periodic
update check. Clients must not retry if they successfully receive a
CUP-validated response from the server, regardless of the contents. Servers may
protect themselves in the case of overload by issuing an X-Retry-After HTTP
header, detailed [below](#headers-update-check-response).

For download requests, clients should prefer to fall back to subsequent download
URLs rather than attempting retries on a particular URL.

### Safe JSON Prefixes
JSON responses from the server are prefixed with `)]}'\n`, where \n indicates a
new line character. This prevents them from being naively executed as a script
in the context of the update server's origin.


## Update Checks
An update check is the first phase of an Omaha session. In an update check, the
client sends information about what applications it is managing and the server
responds with whether any updates are available for the applications.

### URL (Update Check Request)
Update check requests will bear a query parameter as defined by [CUP](cup.md).

### Headers (Update Check Request)
Clients should send the following headers with each request. Request headers are
not protected by CUP and therefore should be considered untrusted data. They are
advisory data replicated from the request body so that server-side DoS
prevention mechanisms can inexpensively reject traffic (i.e. without parsing the
request body).
 *   `X-Goog-Update-Interactivity`: Either "fg" or "bg". "fg" indicating a
     user-initiated foreground update. "bg" indicates that the request is part
     of a background update. If the server is under extremely high load, it may
     use this to prioritize "fg" requests over "bg" requests.
 *   `X-Goog-Update-AppId`: A comma-separated list of application IDs included
     in this request.
 *   `X-Goog-Update-Updater`: "name-version", where "name" is the name of the
     updater as reported in `request.updater` and "version" is the version of
     the updater as reported in `request.updaterversion`.

### Body (Update Check Request)
The request body of an update check contains a JSON object with the following
members:
 *   `request`: A `request` object.

#### `request` Object (Update Check Request)
A request object has the following members:
 *   `@os`: A string identifying the operating system.
     Added for backwards compatibility with Chrome Web store.
     Recommend: new servers should use `os` member instead.
     Default: "". Known values include:
     *   "android": Android.
     *   "cros": ChromeOS.
     *   "fuchsia": Fuchsia.
     *   "linux": Linux.
     *   "mac": macOS.
     *   "openbsd": OpenBSD.
     *   "win": Windows.
 *   `@updater`: A string identifying the client software (e.g. "Omaha",
     "Chrome", "Chrome Extension Updater"). Default: "".
 *   `acceptformat`: A string, formatted as a comma-separated list of strings,
     describing the pipeline operations that this client can perform.
     Default: "".
     The valid operations are any of the types supported by the
     [Operation Object](#operation-objects-update-check-response)
 *   `apps`: A list of `app` objects.
 *   `dedup`: A string, must be "cr". This indicates to servers that the client
     intends to use client-regulated counting algorithms rather than any sort of
     unique identifier. Version 3.0 of the protocol also supported "uid".
 *   `dlpref`: Specifies the preferred download URL behavior. A comma-separated
     list of values, in order of descending priority. Default: "". Legal values
     include:
     *   "" (empty string): No preference.
     *   "cacheable": Proxy-cacheable download URLs are preferred.
 *   `domainjoined`: A boolean. True if the device is a managed enterprise
     device that will respect enterprise group or cloud policies. False
     otherwise.
 *   `hw`: A `hw` object (see below).
 *   `ismachine`: Whether this client is installed system-wide or only for a
     single user. Default: -1. Legal values:
     *   -1: unknown
     *   1: this client is installed in a cross-user context.
     *   0: this client is installed only for the current user.
 *   `os`: An `os` object.
 *   `protocol`: The version of the Omaha protocol. Clients must transmit "4.0"
     as the value when using this protocol.
 *   `requestid`: A randomly-generated string, unique to this request. Default:
     "" (empty string).
 *   `sessionid`: A randomly-generated string, unique to this update session.
     Default: "" (empty string).
 *   `testsource`: A string set in tests, developer requests, or automated
     probers to distinguish this request from real user traffic. Any value
     other than the empty string indicates that the request should not be
     counted toward official metrics. Default: "" (empty string).
 *   `updaters`: A list of `updater` objects.
 *   `updaterchannel`: If present, identifies the distribution channel of the
     client (e.g. "stable", "beta", "dev", "canary", "extended"). Default: "".
 *   `updaterversion`: The version of the client that is sending this request.
     Default: "0".

#### `hw` Objects (Update Check Request)
An `hw` object contains information about the capabilities of the client's
hardware. It has the following members:
 *   `sse`: "1" if the client's hardware supports the SSE instruction set. "0"
     if the client's hardware does not. "-1" if unknown. Default: "-1".
 *   `sse2`: As `sse` but for the SSE2 instruction set extension.
 *   `sse3`: As `sse` but for the SSE3 instruction set extension.
 *   `sse41`: As `sse` but for the SSE4.1 instruction set extension.
 *   `sse42`: As `sse` but for the SSE4.2 instruction set extension.
 *   `ssse3`: As `sse` but for the SSSE3 instruction set extension.
 *   `avx`: As `sse` but for the AVX instruction set extension.
 *   `physmemory`: The physical memory the client has available to it, measured
     in gibibytes, truncated down to the nearest gibibyte, or "-1" if unknown.
     This value is intended to reflect the maximum theoretical storage capacity
     of the client, not including any hard drive or paging to a hard drive or
     peripheral. Default: "-1".

#### `os` Objects (Update Check Request)
An `os` object contains information about the operating system that the client
is running within. It has the following members:
 *   `platform`: The operating system family that the client is running within
     (e.g. "win", "mac", "linux", "ios", "android"), or "" if unknown. The
     operating system family name should be transmitted in a canonical form.
     Formatting varies across implementations. Default: "". Known values:
     *   "android" or "Android": Android.
     *   "chromeos" or "ChromeOS" or "Chrome OS": Chrome OS.
     *   "chromiumos" or "ChromiumOS" or "Chromium OS": Chromium OS.
     *   "dragonfly": DragonFly BSD.
     *   "freebsd" or "FreeBSD": FreeBSD.
     *   "Fuchsia": Fuchsia.
     *   "ios" or "iOS": Apple iOS.
     *   "linux" or "Linux": Linux and its derivatives, except as mentioned
         below.
     *   "mac" or "Mac OS X": Apple macOS and its derivatives.
     *   "openbsd" or "OpenBSD": OpenBSD.
     *   "Solaris": Solaris.
     *   "win" or "Windows": Microsoft Windows and its derivatives.
     *   "Unknown": Sent by some clients instead of "" when the platform is not
         recognized.
 *   `version`: The version number of the operating system, or "" if unknown.
     Default: "".
 *   `sp`: The service pack level of the operating system, or "" if unknown or
     not applicable. Default: "".
     *    On Windows, the service pack should be formatted as a Title Case
          string (e.g. "Service Pack 2").
 *   `arch`: The architecture of the operating system, or "" if unknown.
     Default: "". Known Values:
     *   "arm": ARM
     *   "arm64": 64-bit ARM
     *   "x86": x86
     *   "x86_64": x86-64
     *   "x64": x64

#### `app` Objects (Update Check Request)
Each managed application is represented by exactly one `app` object. It has the
following members:
 *   `appid`: The [application id](#application-ids) that identifies the
     application. Default: Undefined - Clients must transmit this attribute.
 *   `brand`: The brand code corresponding to the partner promotion that
     triggered the installation of the application. Brand codes usually match
     the [A-Z]{4} regex, but the protocol supports any string. Default: "".
 *   `cohort`: A machine-readable string identifying the release cohort
     (channel) that the app belongs to. Limited to ASCII characters 32 to 126
     (inclusive) and a maximum length of 1024 characters. Default: "".
 *   `cohorthint`: A value that expresses a client request to move to another
     release cohort. The exact values may be application-specific and should be
     set up ahead of time on the server. Default: "".
 *   `cohortname`: A human-readable string identifying the semantics behind the
     current cohort. For example, this might be displayed to the user and
     indicate the channel or experimental status. Default: "".
 *   `release_channel`: The target channel that the app switches to. For
      example an app can have stable, beta, dev, and canary channels. Note
      switching to an older channel may have no effect until the older channel
      catches up with the install. Ex: a machine on today's beta (107.0.5304.62)
      that is switched to stable will stay on that version until 107 ships to
      stable. Downgrade can be forced by the use of the `rollback_allowed` in
      the `updatecheck` node.
 *   `data`: A list of `data` objects.
 *   `disabled`: A list of `disabled` objects.
 *   `enabled`: Indicates whether the application is enabled on the client. As
     an example, Chrome extensions can be put into a "disabled" state without
     uninstallation. A value of "-1" indicates that the enabled status is
     unknown, or that the concept of enabling/disabling does not exist. "0"
     indicates that the application is disabled. "1" indicates that the app is
     enabled.  Default: "-1".
 *   `cached_items`: A list of `hash` objects, identifying files that the
     client has available to use for the purposes of differential updates. This
     list may be empty, for example if the local cache is empty or does not
     exist.
 *   `iid`: Installation ID is an opaque token that identifies an installation
     flow. The installation ID is a unique identifier embedded into a
     metainstaller for the application. It can be used to correlate the first
     update-check of an application with web actions and the application
     download. To avoid creating a persistent unique identifier for this
     installation, clients must clear the `iid` value after transmitting it
     once. Default: "".
 *   `installdate`: The approximate date that the application installation took
     place on, or "-2" if unknown or not applicable. Default: "-2". During the
     installation request itself (the first communication to the server), the
     client should use a special value of "-1". The
     `response.daystart.elapsed_days` value for that request's response should
     be stored to use in all subsequent requests. To mitigate privacy risk,
     clients should fuzz the value to the week granularity by storing X - X % 7,
     where X is the server-provided date. For offline installs, the client
     should send -2. Default: -2.
 *   `installedby`: A string describing the original cause of the installation.
     The string should be drawn from a small set of constant values, to minimize
     entropy and the ability for the client to be fingerprinted. Default: "".
 *   `installsource`: A string describing the immediate cause of this request.
     Known values include: "" (a normal background update) and "ondemand" (a
     foreground, user-initiated update). Default: "".
     The string should be drawn from a small set of constant values, to minimize
     entropy and the ability for the client to be fingerprinted.
 *   `ismachine`: "0" if the application is installed for the user specifically
     (i.e. elevated privileges are not needed to update it). "1" if the
     application is installed in a cross-user (system or privileged) context.
     "-1" if unknown or not applicable. Default: "-1"
 *   `lang`: The language of the application installation, in BCP 47
     representation, or "" if unknown or not applicable. Default: "".
 *   `ping`: A `ping` object.
 *   `tag`: A string, representing arbitrary application-specific data that the
     client wishes to disclose to the server. Clients should prefer to extend
     this protocol with additional attributes rather than use this field, but in
     some cases transmission of opaque or unparsable data is necessary (for
     example, transmitting the tag of a tagged metainstaller). Default: "".
 *   `version`: The [version number](#version-numbers) of the current
     installation of the application. A successful installation of an update
     should change this version in future requests, even if the old version of
     the application may still be running.
     Default: "0".
 *   `updatecheck`: An `updatecheck` object. This member may be omitted if the
     client will not honor an update response.

#### `hash` Objects (Update Check Request and Update Check Response)
 *   `sha256`: A SHA-256 hash, rendered in lowercase base16.

#### `ping` Objects (Update Check Request)
 *   `ad`: The date that the previous active report took place on, or "-1" if
     this is the first active report for the application. "-2" if the date of
     the previous active report is unknown or the app has not been active since
     the previous active report. Default: "-2". If not "-2", this request is an
     active report. If "-2" but the application is known to have been active,
     this request is an active report. In response to the previous active
     report, the server responded with the date in the `response.clock.date`
     member. See [User Counting](#user-counting).
 *   `rd`: The date that the previous roll call took place on, or "-1" if this
     is the application's first roll call, or "-2" if the date of the previous
     roll call is not known. Default: "-2". This request is a roll call,
     regardless of the value of this member. In response to the previous roll
     call, the server responded with the date in the `response.clock.date`
     member. See [User Counting](#user-counting).
 *   `ping_freshness`: A random 128-bit number, written into the client's
     storage alongside the next value of rd, and rotated whenever the client
     stores a new value for rd. See [Ping Freshess](#ping-freshness). A value of
     "" (empty string) indicates that no value was available. Default: "".

#### `data` Objects (Update Check Request)
A data object represents a request for arbitrary specific application-specific
data from the server. The server maintains a map of index values to data
contents, and can supply them if requested. This is used during installation to
transmit alternate branding or seeded configurations to the application. `data`
objects have the following members:
 *   `name`: The type of data lookup to perform. The only known supported value
      is "install". Default: "".
 *   `index`: The key to look up on the server. Default: "".

#### `disabled` Objects (Update Check Request)
A disabled object contains information about why an app is disabled. Multiple
causes for a disabled state may exist.
 *   `reason`: an numeric reason that the app is disabled. The meaning of these
     values are client-specific (possibly app-specific), except for "0", which
     indicates no reason. Default: "0".

#### `updatecheck` Objects (Update Check Request)
An updatecheck object represents the actual intent to update. It has the
following members:
 *   `rollback_allowed`: true if the client will accept a version downgrade.
     Typically used in conjunction with a targetversionprefix. Default: false.
 *   `sameversionupdate`: true if the client is requesting that it be updated
     to the version it currently has (usually as part of a repair or
     overinstallation) instead of receiving no update. Default: false.
 *   `targetversionprefix`: A component-wise prefix of a version number, or a
     complete version number. The server SHOULD NOT return an update
     instruction to a version number that does not match the prefix or complete
     version number. The prefix is interpreted a dotted-tuple that specifies
     the exactly-matching elements; it is not a lexical prefix. (For example,
     "1.2.3" matches "1.2.3.4" but not "1.2.34".) Default: "".
 *   `updatedisabled`: An indication of whether the client will honor an update
     response, if it receives one. Legal values are "true" (indicating that the
     client will ignore any update instruction) and "false" (indicating that the
     client will attempt an update if one is instructed). Default: "false".

#### `updater` Objects (Update Check Request)
An updater object represents the state of another sibling update program on
the system. Clients report about other updaters present on the system to enable
redundancy and recoverability of sibling updaters. For example, Chrome is
normally updated by GoogleUpdate, but in cases where GoogleUpdate has been
disabled or is broken (according to the data transmitted here), the server can
issue a pipeline for Chrome to attempt recovery of GoogleUpdate. An updater
object has the following members:
 *   `autoupdatecheckenabled`: 1 if the other updater is subject
     to an enterprise policy that disables its update-checking functionality. 0
     if not. -1 if unknown. Default: -1.
 *   `ismachine`: 1 if the updater has system or administrator
     privileges, or if it is installed in a cross-user context. 0 if not. -1 if
     unknown. Default: -1.
 *   `lastchecked`: An estimated number of hours since the other updater
     successfully checked for an update. A value of -1 indicates the last check
     time is unknown. Default: -1. Clients should limit the accuracy of this
     value in order to prevent an observer from correlating this request to the
     one last sent by the subject updater.
     >Chrome only sends the following values:
     >*   -1: unknown
     >*   0: [0, 336) hours ago (0 to < ~2 weeks)
     >*   336: [336, 1344) hours ago (~2 weeks to ~2×28 days)
     >*   1344: at least 1344 hours ago (~2×28 days or more)
 *   `lastupdatecheckerrorcat`: The numeric error category encountered on
     the last update check. 0 for success. Default: "0".
 *   `lastupdatecheckerrorcode`: The numeric error code encountered on the last
     update check. 0 for success. Default: "0".
 *   `lastupdatecheckextracode1`: The numeric extra code encountered on the
     last update check. 0 for success. Default: "0".
 *   `laststarted`: An estimated number of hours since the other updater
     successfully ran (started and exited without crashing). A value of -1
     indicates the last check time is unknown. Default: -1. Clients should
     limit the accuracy of this value in order to prevent an observer from
     correlating this request to the one last sent by the subject updater.
     >Chrome only sends the following values:
     >*   -1: unknown
     >*   0: [0, 336) hours ago (0 to < ~2 weeks)
     >*   336: [336, 1344) hours ago (~2 weeks to ~2×28 days)
     >*   1344: at least 1344 hours ago (~2×28 days or more)
 *   `name`: The name of the other updater.
 *   `updatepolicy`: A numeric indicator of what kind of updater policy the
     other updater has about updating this client. Default: -2. Known values:
     *   -2: unknown
     *   -1: no such policy exists
     *    0: policy set to allow background and foreground updates.
     *    1: policy set to allow background updates only.
     *    2: policy set to allow foreground updates only.
     *    3: policy set to disable updates.
 *   `version`: The current version number of the other updater. Default: "0".

---

### Headers (Update Check Response)
In addition to normal HTTP headers, this protocol defines the following update
check response headers:
 *   `X-Cup-Server-Proof`: Contains the CUP signature of the response.
 *   `X-Retry-After`: If present, a positive integer, representing a number of
     seconds for which the client mut not contact the server again for
     background updates, including but not limited to retries of this particular
     request. Clients must respect this header even if paired with
     non-successful HTTP response code. Servers should not send a value in
     excess of 86400 (24 hours), and clients should treat values greater than
     86400 as 86400. Clients may still send a ping-back for this update session.

### Body (Update Check Response)
The response body of an update check begins with `)]}'\n`, where \n indicates a
newline character, followed by a JSON object with the following members:
 *   `response`: A `response` object.

#### `response` Objects (Update Check Response)
A response object contains the server's response to a corresponding `request`
object in the update check request.
 *   `apps`: A list of `app` objects. There is one object for each `app` in the
     request body.
 *   `daystart`: A `daystart` object.
 *   `protocol`: The version of the Omaha protocol. Servers responding with this
     protocol must send a value of "4.0".
 *   `server`: A string identifying the server or server family for diagnostic
      purposes. As examples, "production", "test". Default: "".

#### `daystart` Objects (Update Check Response)
A clock object contains information about the current datetime according to the
server's locale. It has the following members:
 *   `elapsed_days`: An integer. The number of complete calendar days that have
     elapsed since January 1st, 2007 in the server's locale, at the time the
     request was received. The client should generally save this value for use
     in future update checks (for examples, see `request.app.ping.rd` and
     `request.app.installdate`).

#### `app` Objects (Update Check Response)
An app object represents a per-application acknowledgement of the request. If an
application appears in the request, it must have a corresponding acknowledgement
in the response. It has the following members:
 *   `appid`: The [application ID](#application-ids) identifies the application.
     Servers must always transmit this attribute.
 *   `cohort`: The new cohort value for the application. The client should
     update the cohort of this application to the new value. Subject to the same
     length and language limitations as `request.app.cohort`. Default: "" (empty
     string).
 *   `cohorthint`: The new cohort hint value for the application. The client
     should update the cohorthint of this application to this value. Subject to
     the same length and language limitations as `request.app.cohorthint`.
     Default: "" (empty string).
 *   `cohortname`: The new cohort name value for the application. The client
     should update the cohortname of this application to this value. Subject to
     the same length and language limitations as `request.app.cohortname`.
     Default: "" (empty string).
 *   `data`: A list of `data` objects.
 *   `updatecheck`: An `updatecheck` object, if one was sent in the request.
 *   `status`: The state of the product on the server. Default: "ok". Known
     values:
     *   "ok": The application is recognized.
     *   "restricted": The application is recognized, but due to policy
         restrictions (such as export law compliance) the server must refuse to
         give a meaningful response.
     *   "error-unknownApplication": The server is not aware of an application
         with this ID.

#### `data` Objects (Update Check Response)
Each data object in the response represents an answer to a data request from the
client. It has the following members:
 *   `name`: The requested data name from `request.app.data.name`, echoed back
     to the client.
 *   `index`: The requested data index from `request.app.data.index`, echoed
     back to the client, if valid.
 *   `status`: The outcome of the data lookup. Default: "ok". Known values:
     *   "ok": This tag contains the appropriate data response, even if such a
         response is the empty string.
     *   "error-invalidargs": The data request could not be parsed or
         understood.
     *   "error-nodata": The data request was understood, but the server does
         not have a value for the requested entry. (This is distinct from having
         a zero-length value.)
 *   `#text`: The value of the requested data index.

#### `updatecheck` Objects (Update Check Response)
An updatecheck response object contains whether or not there is an update
available for the application, and if so, the instructions to install it. It has
the following members:
 *   `info`: An optional string that provides a rationale for the status
     response, for use in debugging.  For example, "update disabled by client"
     or "bandwidth limit exceeded".  Default: "".
 *   `status`: Indicates the outcome of the updatecheck. Servers must always
     send a value here. Known values:
     *   "ok": An update is available and should be applied.
     *   "noupdate": No update is available for this application at this time.
     *   "error-internal": The server encountered an unspecified internal error.
     *   "error-hash": The server attempted to serve an update, but could not
         provide a valid hash for the download.
     *   "error-osnotsupported": The server recognized the application, but the
         client does not meet the minimum system requirements to receive any
         version of the application. In particular, it is running on an
         incompatible operating system.
     *   "error-hwnotsupported": The server recognized the application, but the
         client does not meet the minimum system requirements to receive any
         version of the application. In particular, it is running on
         incompatible hardware.
     *   "error-unsupportedprotocol": This application is incompatible with this
         version of the protocol. (For example, it may require multi-package
         support from the Omaha 3 protocol, or may require some feature added in
         a later version of this protocol.)
     *   "error-inexpressible": The server finds that it is unable to produce a
         list of pipelines for the given product using the set of operations
         provided in `acceptformat`.

The following members are only present if the `status` is "ok":
 *   `nextversion`: The expected version of the product, if any pipeline is
     able to complete all operations successfully.
 *   `pipelines`: A list of `pipeline` objects.

#### `pipeline` Objects (Update Check Response)
A pipeline object describes a pipeline process that may be applied in order to
update the current binary. A pipeline is represented as a series of operations.
A pipeline object has the following members:
 *  `pipeline_id`: A string describing the pipeline strategy. This string is
    echoed back to the server in `event` objects to help the server attribute
    events to a particular pipeline. Pipeline IDs are not necessarily globally
    unique; for example, a server might use an ID of "zucc:h1sha256->h2sha256"
    to identify a pipeline that updates an application from h1 to h2, using a
    zucchini patch, and reuse that ID across many update check responses.
 *  `operations`: A list of `operation` objects.

#### `operation` Objects (Update Check Response)
A operation object describes one of many operations to be performed in order to
produce, process, and eventually install a update.

It has the following members:
 *  `type`: The type of operation to execute, represented as a string. This can
    be any of the operations provided by `acceptformat` to the
    [Request Object](#request-object-update-check-request).

For `type == "download"`: Download a payload.
 *  `size`: The size in bytes of the payload requested for download.
    This field is required and must be a value greater than 0.
 *  `out`: A `hash` object containing the expected hash of the downloaded
    bytes. This field is required.
 *  `urls`: The ordered list of `url` objects from which this payload may be
    obtained. Clients must attempt to download from each URL of the appropriate
    type in the specified order, falling back to the next URL if a TCP or HTTP
    error is encountered. A 4xx or 5xx HTTP response qualifies as an error that
    justifies a fallback. A successful download of a file that fails to hash to
    the provided `outhash_sha256` or has an unexpected size also qualifies.
    Other network errors may also qualify. This field is required to be set and
    contain at least one url for the operation to be processed properly.

For `type == "xz"`: Decompress an xz file produced by the previous operation.
    The file is compressed using the
    [XZ File Format](https://tukaani.org/xz/xz-file-format.txt).

For `type == "zucc"`: Apply a differential Zucchini patch produced by a
    previous operation to a cached payload. The patch is generated using
    [Zucchini](https://chromium.googlesource.com/chromium/src.git/+/main/components/zucchini/README.md).
 *  `previous`: A `hash` object representing the file to apply this patch to.
    This field is required.
 *  `out`: A `hash` object representing the file produced from this patch. This
    field is required.

For `type == "puff"`: Apply a differential Puffin patch produced by a previous
    operation to a payload stored in the cache. The patch is generated using
    [Puffin](https://chromium.googlesource.com/chromium/src.git/+/main/third_party/puffin/README.md).
 *  `previous`: A `hash` object representing the file to apply this patch to.
    This field is required.
 *  `out`: A `hash` object representing the file produced from this patch. This
    field is required.

For `type == "crx3"`: Decompress a CRX3 package produced by the previous
    operation and install it. The precise handling of the installation varies
    depending on the updater and the operating system.
 *  `in`: A `hash` object containing the expected hash of the CRX3 to be
    installed. This field is required.
 *  `path`: The path to the installer executable(s), relative to the root of the
    CRX. Clients that install content without running an embedded executable
    (such as the Chrome extension updater) should ignore this value. Otherwise,
    on Windows this value indicates a single executable that the updater should
    run to complete the installation. On macOS, this value indicates a directory
    where install executables (`.preinstall`, `.install`, `.postinstall`, and
    their [Keystone equivalents](installer_api_mac.md)) are located.
    Additionally, some values have special handling on macOS:
     *  A value in the form `path/to/basename.dmg` indicates a disk image file
        whose root directory contains the install executables.
     *  A value in the form `path/to/basename.app` indicates a application
        bundle that shares a parent directory with the install executables.
     *  Since 138.0.7182.0, any other values that have no file extension,
        including `.`, indicate a directory containing the install executables.
 *  `arguments`: A string indicating command-line arguments that should be
    passed to the identified binary to execute.

For `type == "run"`: Execute a binary located at a given path.
 *  `path`: The path to the executable relative to the directory that the
    application is currently installed into. If the application was updated
    earlier in this pipeline, this path is relative to the new installation.
 *  `arguments`: The command line arguments to be passed to the executable,
    formatted as a single string.

#### `url` Objects (Update Check Response)
A url object describes a fully-qualified URL. It has the following members:
 *   `url`: The fully-qualified URL as a string.

---

## Downloads
Download requests occur when an application update is needed, as a result of a
`response.app.updatecheck.pipeline.operation.url` list element. Download
requests are HTTP GET requests and can use any HTTP implementation.

### Request Headers
In addition to the regular HTTP headers, this protocol defines the following
headers for the download request:
 *   `X-Goog-Update-SessionId`: The [session ID](#session-ids) of the update
     session.

---

## Ping-Backs
Ping-back requests are caused by any operation attempted during an update
session. Ping-backs share a similar structure to update check requests.

Ping-back requests are fire-and-forget: the client can discard the server's
response. Ping-back transactions need to be protected by CUP if and only if the
client handles the server's response.

Ping-back requests can be **bundled** into a single HTTP transaction, or sent
immediately as the events triggering the ping-back occurs. Bundling ping-backs
reduces QPS to the server, but risks ping-back loss if the client crashes,
loses connectivity, or loses power before it transmits the bundle.

### Ping-Back Request Headers
Similarly to update-check requests, ping-back requests have additional headers
for the purpose of easier DoS rejection.
 *   `X-Goog-Update-AppId`: A comma-separated list of application IDs included
     in this request.
 *   `X-Goog-Update-Updater`: "name-version", where "name" is the name of the
     updater as reported in `request.updater` and "version" is the version of
     the updater as reported in `request.updaterversion`.

### Ping-Back Request Body
The request body of an ping-back contains a JSON object with the following
members:
 *   `request`: A `request` object.

#### `request` Objects (Ping-Back Request)
A ping-back `request` object contains all the same members as a update check
`request` object. Differences occur in the `request.app` objects.

#### `app` Objects (Ping-Back Request)
A ping-back `app` object is identical to an update check `app` object, except
for the following differences.

A ping-back `app` object cannot contain any of the following members:
 *   `data`
 *   `ping`
 *   `updatecheck`

A ping-back `app` additionally contains the following members:
 *   `events`: a list of `event` objects.

#### `event` Objects (Ping-Back Request)
An event object represents a specific report about an operation the client
attmpted as part of this update session. All events have the following members:
 *   `eventtype`: The event type is a numeric value indicating the type of the
     event. It must always be specified by the client. The following values are
     known:
     *   2: An install session.
     *   3: An update session.
     *   4: An uninstall session.
     *   14: A `download` operation.
     *   60: An `xz` operation.
     *   61: A `zucc` patch application operation.
     *   62: A `puff` patch application operation.
     *   63: A `crx3` package installation operation.
     *   64: An unknown operation.
     *   41: An app command completion event.
     *   42: A `run` operation.
 *   `eventresult`: The outcome of the operation. Default: 0. Known values:
     *   0: error
     *   1: success
     *   4: cancelled
 *   `errorcat`: An error category, for use in distinguishing between different
     classes of error codes. Default: 0. The following values are known:
     *   0: No category.
     *   1: Errors acquiring the download.
     *   2: Errors during CRX unpacking.
     *   3: Update client errors during installation.
     *   4: Errors within the update service itself.
     *   5: Error during update check.
     *   6: Reserved.
     *   7: Application installer errors during installation.
     *   8: Errors encountered during patch application.
 *   `errorcode`: The error code (if any) of the operation. Default: 0. The
     meaning of an error code may depend on the error category. 0 always means
     "no error" (success).
     *   Additional values may exist in
         (update_client_errors.h)[https://cs.chromium.org/chromium/src/components/update_client/update_client_errors.h]
 *   `extracode1`: Additional numeric information about the operation's result.
     The meaning of an extra code depends on the error category and error code.
     Default: 0.

Depending on the event type, additional members may be present:

For `eventtype == 2` events:
 *   `nextversion`: The application version that the client was attempting to
     update to, regardless of whether the update was successful.

For `eventtype == 3` events:
 *   All the members of `eventtype == 2` events.
 *   `previousversion`: The application version the client had prior to the
     update, regardless of whether that update was successful.

For `eventtype == 14` events:
 *   `download_time_ms`: The time elapsed between the start of the download and
     the end of the download, in milliseconds. -1 if unavailable. Default: -1.
 *   `downloaded`: The number of bytes successfully received from the download
     server. Default: 0.
 *   `downloader`: A string identifying the download algorithm / stack. Known
     values:
     *   "" (empty string): Unknown downloader.
     *   "nsurlsession_background": MacOS background NSURLSession.
     *   "bits": Microsoft BITS.
     *   "direct": The Chromium network stack.
 *   `total`: The size in bytes of the payload provided in the given url.
     Default: 0.
 *   `pipeline_id`: The `pipeline_id` set in the request for this operation's
     pipeline.
 *   `url`: The URL from which the download was attempted.

For `eventtype == 60, 61, 62, 63, or 64` events:
 *   All the members of `eventtype == 3` events.
 *   `pipeline_id`: The `pipeline_id` set in the request for this operation's
     pipeline.

For `eventtype == 41` events:
 *   `appcommandid`: The id of the app command for which the ping is being sent.

For `eventtype == 42` events:
 *   `actiontype`: The type of the action that caused this event.

### Ping-Back Response Body
Clients are free to ignore the ping-back response body. However, to allow future
extensions in the ping-back response, the protocol defines a basic ping-back
response body. Clients that do not ignore the ping-back response must protect
the ping request using CUP.

The response body of an ping-back contains a JSON object with the following
members:
 *   `response`: A `response` object.

#### `response` Objects (Ping-Back Response)
A ping-back `response` object contains all the same members as a update check
`response` object. Differences occur in the `response.app` objects.

#### `app` Objects (Ping-Back Response)
A ping-back response `app` object is identical to an update check response `app`
object, except for the following differences.

A ping-back `app` object cannot contain any of the following members:
 *   `data`
 *   `updatecheck`

A ping-back `app` additionally contains the following members:
 *   `events`: a list of `event` objects.

#### `event` Objects (Ping-Back Response)
A ping-back response `event` object indicates the server's acknowledgement of
the event. It has the following members:
 *   `status`: Indicates the result of parsing the action on the server side.
     Known values:
     *   "ok": The server acknowledges successful receipt of this event ping.
