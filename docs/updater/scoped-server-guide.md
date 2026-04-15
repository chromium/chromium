# Using `ScopedServer` in Chromium Updater Integration Tests

`ScopedServer` is a mock local HTTP server used in the Chromium Updater's
integration tests (`chrome/updater/test/integration_tests.cc`). It provides a
means for mocking out the Omaha service during tests. If it receives an
unmatched request, or it has not seen all the requests it expected by the time
it is destroyed, it reports a test failure.

This document serves as a detailed reference on how to set it up, when to use
it, and what helpers are available to avoid writing manual request matchers.

## What is `ScopedServer`?

Under the hood, `ScopedServer` wraps a `net::test_server::EmbeddedTestServer`.
When instantiated, it starts serving locally on an available port and configures
the Updater running in the integration test environment to route its traffic
(update checks, pings, crash uploads, etc.) there.

**Key characteristics:**
1. **Strict Mocking:** If `ScopedServer` receives any request it was not
   expecting, it fails the test and serves an HTTP 500 response.
2. **Sequential Expectations:** `ScopedServer` maintains an ordered queue of
   pending expectations. Incoming requests are tested only against the front of
   the queue. If requests arrive out-of-order, the test fails.
3. **Verification upon destruction:** When the `ScopedServer` goes out of scope
   and is destructed, it verifies that *all* expected requests were successfully
   received. If there are pending expectations, it fails the test.
4. **No cleanup:** The updater's original configuration is not restored; it will
   continue to attempt to use the test server after it has been torn down if it
   is not explicitly reconfigured.

## Setup

Instantiating `ScopedServer` with your test's `IntegrationTestCommands` object
is generally adequate to set it up. Tests using more than one such object will
need to call `IntegrationTestCommands::EnterTestMode` on those instances not
passed to the `ScopedServer` constructor; see
`IntegrationTestUserInSystem::SetUp` for an example of this.

### When to set it up

Instantiate `ScopedServer` when you wish to verify or respond to messages sent
between the updater and the Omaha server during a test. A `ScopedServer` is
strictly required for test scenarios where the client is required to transact
with the server -- for example, update checks.

`ScopedServer` construction can be deferred until partway through a test in some
cases. Installing the updater, for example, does not require successfully
reaching the Omaha server. Since `ScopedServer` reports a test failure if it
receives any unexpected traffic, deferring setup can save boilerplate.

### Test fixtures that already set it up

`IntegrationTestUserInSystem`, and its subclass
`IntegrationTestKSAdminFourApps`, both set up a `ScopedServer` during `SetUp`.
They handle requests from _both_ `IntegrationTestCommands` instances in each
fixture. This isolates the boilerplate required for setting up the second
updater to talk to the test server.

## Omaha Protocol Helpers

Instead of manually using `test_server.ExpectOnce()` with
`request::GetJSONContentMatcher(…)` for every step of the Omaha protocol, prefer
the helper methods available in `integration_tests.cc` and
`IntegrationTestCommands` when relevant. Business logic for these helpers is
largely implemented in `integration_tests_impl.cc`.

This list may not be complete or up-to-date -- it was last updated on
2026-05-07.

### Comprehensive Sequence Helpers

These expect a full series of requests (e.g. an update check, follow-up pings,
and optionally downloads).

* **`ExpectUpdateSequence`** / **`ExpectInstallSequence`**
  ```cpp
  void ExpectUpdateSequence(ScopedServer& test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version);
  ```
  Sets up the expectation for a successful update or install check, serves the
  update metadata, and dynamically generates the mock installer payload.

* **`ExpectSelfUpdateSequence`**
  ```cpp
  void ExpectSelfUpdateSequence(ScopedServer& test_server);
  ```
  Configures expectations for the Updater updating *itself*.

* **`ExpectUpdateSequenceBadHash`** Expects an update sequence but serves an
  installer payload whose hash deliberately does not match the manifest to test
  fallback/error scenarios.

### Single-Step Request Helpers

* **`ExpectUpdateCheckRequest(test_server)`** Expects the updater to send a
  check request, responds with standard protocol JSON saying "no updates
  available".

* **`ExpectNoUpdateSequence(test_server, app_id, updater_version,
  app_version)`** Expects an update check from a specific app (optionally, of a
  specific version) from a specific version of the updater (defaulting to
  `kUpdaterVersion`). Provides a "no update" response.

* **`ExpectPing(test_server, event_type, target_url)`** A general-purpose helper
  to expect an Omaha event ping of some type.  Event codes are defined in
  `components/update_client/protocol_definition.h`.

* **`ExpectPingRequest(test_server, app_id, ping_params, version)`** Expects an
 event ping for a specific app, with specified parameters. (If
 `ping_params.extra_code1` is 0, it is not checked.) `version` refers to the
 version of the updater. Responds with `"status":"ok"`.

* **`ExpectUninstallPing(test_server)`** Expects a standard uninstaller ping
  (`eventtype: 4`).

* **`ExpectUninstallPingPreviousVersion(test_server, previous_version)`**
  Expects an uninstall ping for a specific prior version of the updater.

* **`ExpectInstallEvent(test_server, app_id)`** Expects an event ping where
  `eventtype` is `2` (`kEventInstall`).

* **`ExpectAppErrorEvent(test_server, app_id, error_code, event_type)`**
  Windows-only. Expects an event ping reporting a specific installation error.

* **`ExpectAppCommandPing(test_server, appid, appcommandid, errorcode,
  eventresult, event_type, ...)`** Verifies that the updater sent a ping
  detailing the result of launching a specific AppCommand.

* **`ExpectInstallSource(test_server, install_source)`** Expects an install ping
  with a specific `installsource="..."` tag (e.g. `ondemand`, `offline`).

## Writing Custom Matchers

If the pre-built helpers don't cover your scenario, write custom matchers using
`test_server.ExpectOnce()`. Core request-matching primitives are defined in
`chrome/updater/test/request_matcher.h`.

The `ExpectOnce` method requires:

1. **The Request Matcher Group:** A list of conditions the incoming HTTP request
   must satisfy (e.g., path, headers, regex content matches). The anticipated
   request must satisfy _all_ matchers in this list.
2. **The Output Response:** The body that the `ScopedServer` should reply with
   when the request matches. The HTTP status code can be provided (as a
   `net::HttpStatusCode`) as an optional third argument; it defaults to HTTP
   200. (Nonmatching requests always receive an error 500 and a test failure.)

### Providing the Response

There are two ways to provide the output response:

**1. As a static string:** If the reply is the same in both Omaha V3.1 and Omaha
V4, or the test does not need to support both protocols, provide an expectation
as a string:
```cpp
test_server.ExpectOnce(
    {
      request::GetUpdaterUserAgentMatcher(),
      request::GetContentMatcher({
          R"(.*"appid":"my_custom_app".*)",
          R"(.*"eventtype":5.*)"
      })
    },
    ")]}'\n{\"response\":{\"protocol\":\"4.0\",\"apps\":[{\"appid\":\""
    "my_custom_app\",\"status\":\"ok\"}]}}"
);
```

**2. As a `base::RepeatingCallback<std::string(bool)>`:** This form of
`ExpectOnce` allows a common implementation to support both Omaha Protocol 4 and
Omaha Protocol 3.1. `ScopedServer` invokes this callback with a boolean
`is_protocol_4`, indicating the protocol version parsed from the request, if the
request successfully matches.

#### Examples

Helpers in  `integration_tests_impl.cc`  may be useful (via
`IntegrationTestCommands` objects), or serve as useful references when
implementing a new one. Some highlights:

* `GetUpdateResponse(app_id, ...)`: Dynamically generates a valid update check
  response (v3.1 or v4.0 depending on the `bool` argument passed to it by the
  test server) complete with a specific installation relative path, arguments,
  and manifest hash.
* `GetUpdateResponseV4(app_responses)` / `GetUpdateResponseV3(app_responses)`:
  Wraps a collection of individual app responses in the standard Omaha envelope.
* `GetUpdateResponseForAppV4(...)`: Returns the specific JSON snippet for a
  single app's update metadata.

## Troubleshooting

* **Strict Ordering:** `ScopedServer` has no "default" responses for any
  requests and no flexibility regarding request order. Anticipate a batch of
  event pings from the setup phase of your test before the primary behavior you
  intended to test. If you delayed `ScopedServer` setup to skip these, you may
  see a single "ping backlog" message when the updater next wakes.
* **Preemptive Expectations:** The various `Expect...` helpers must be called
  *before* the action that triggers the network request. Because these helpers
  configure what the server will reply with, they need to be strictly
  established beforehand, which can be surprising if you're used to
  "verify-after-run" mocking frameworks.
* **Commonly Overlooked Pings:** Tests that appear to complete all expectations
  but crash during cleanup are likely missing a call to `ExpectUninstallPing`
  (or `ExpectUninstallPingPreviousVersion`).
