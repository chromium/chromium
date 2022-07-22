// META: script=/common/subset-tests-by-key.js
// META: script=/common/utils.js
// META: script=resources/support.sub.js
// META: variant=?include=from-private
// META: variant=?include=from-public
// META: variant=?include=from-treat-as-public
//
// Spec: https://wicg.github.io/private-network-access
//
// These tests verify that secure contexts can fetch non-secure subresources
// from more private address spaces, provided without mixed context check.

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

// Generates tests of preflight behavior for a single (source, target) pair.
//
// Scenarios:
//
//  - cors mode:
//    - without targetAddressSpace option
//    - with incorrect targetAddressSpace option
//    - success
//    - success with PUT method (non-"simple" request)
//  - no-cors mode:
//    - success
//
function makePreflightTests({
  subsetKey,
  source,
  sourceDescription,
  targetServer,
  targetDescription,
  targetAddressSpace,
  expectation,
  expectedMessage,
}) {
  const prefix =
      `${sourceDescription} to ${targetDescription} with ${targetAddressSpace} targetAddressSpace option: `;

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      server: targetServer,
      behavior: {
        preflight: PreflightBehavior.success(token()),
        response: ResponseBehavior.allowCrossOrigin(),
      },
    },
    fetchOptions: { targetAddressSpace: targetAddressSpace },
    expected: expectation,
  }), prefix + expectedMessage + ".");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      server: targetServer,
      behavior: {
        preflight: PreflightBehavior.success(token()),
        response: ResponseBehavior.allowCrossOrigin(),
      },
    },
    fetchOptions: { method: "PUT", targetAddressSpace: targetAddressSpace },
    expected: FetchTestResult.FAILURE,
  }), prefix + "PUT " + expectedMessage + ".");
}

// Source: public secure context.
//
// Fetches to the local and private address spaces require a successful
// preflight response carrying a PNA-specific header.

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "local",
  expectation: FetchTestResult.SUCCESS,
  expectedMessage: "success",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "private",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "local",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "private",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "local",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "private",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-private",
  source: { server: Server.HTTPS_PRIVATE },
  sourceDescription: "private",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "local",
  expectation: FetchTestResult.SUCCESS,
  expectedMessage: "success",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "private",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_LOCAL,
  targetDescription: "local",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "local",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "private",
  expectation: FetchTestResult.SUCCESS,
  expectedMessage: "success",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PRIVATE,
  targetDescription: "private",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "local",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "private",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { server: Server.HTTPS_PUBLIC },
  sourceDescription: "public",
  targetServer: Server.HTTP_PUBLIC,
  targetDescription: "public",
  targetAddressSpace: "public",
  expectation: FetchTestResult.FAILURE,
  expectedMessage: "failed",
});

// These tests verify that documents fetched from the `local` address space yet
// carrying the `treat-as-public-address` CSP directive are treated as if they
// had been fetched from the `public` address space.

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    server: Server.HTTPS_PUBLIC,
    treatAsPublic: true,
  },
  target: {
    server: Server.HTTP_PRIVATE,
    behavior: {
      preflight: PreflightBehavior.success(token()),
      response: ResponseBehavior.allowCrossOrigin(),
    },
  },
  fetchOptions: { targetAddressSpace: "private" },
  expected: FetchTestResult.SUCCESS,
}), "treat-as-public-address to private: success.");

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    server: Server.HTTPS_PUBLIC,
    treatAsPublic: true,
  },
  target: {
    server: Server.HTTP_LOCAL,
    behavior: {
      preflight: PreflightBehavior.success(token()),
      response: ResponseBehavior.allowCrossOrigin(),
    },
  },
  fetchOptions: { method: "PUT", targetAddressSpace: "local" },
  expected: FetchTestResult.SUCCESS,
}), "treat-as-public-address to local: success.");
