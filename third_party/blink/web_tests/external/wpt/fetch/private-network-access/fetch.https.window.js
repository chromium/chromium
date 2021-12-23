// META: script=/common/subset-tests-by-key.js
// META: script=/common/utils.js
// META: script=resources/support.js
// META: script=resources/ports.sub.js
// META: variant=?include=baseline
// META: variant=?include=from-local
// META: variant=?include=from-private
// META: variant=?include=from-public
// META: variant=?include=from-treat-as-public
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// These tests verify that secure contexts can fetch subresources from all
// address spaces, provided that the target server, if more private than the
// initiator, respond affirmatively to preflight requests.
//
// This file covers only those tests that must execute in a secure context.
// Other tests are defined in: fetch.window.js

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

// Source: secure local context.
//
// All fetches unaffected by Private Network Access.

subsetTestByKey("from-local", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "local to local: no preflight required.");

subsetTestByKey("from-local", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: {
    port: kPorts.httpsPrivate,
    searchParams: { "final-headers": "cors" },
  },
  expected: kFetchTestResult.success,
}), "local to private: no preflight required.");


subsetTestByKey("from-local", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: {
    port: kPorts.httpsPublic,
    searchParams: { "final-headers": "cors" },
  },
  expected: kFetchTestResult.success,
}), "local to public: no preflight required.");

// Strictly speaking, the following two tests do not exercise PNA-specific
// logic, but they serve as a baseline for comparison, ensuring that non-PNA
// preflight requests are sent and handled as expected.

subsetTestByKey("baseline", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: {
    port: kPorts.httpsPublic,
    searchParams: {
      // Missing "preflight-uuid" param: preflight will fail.
      "preflight-headers": "cors",
      "final-headers": "cors",
    },
  },
  fetchOptions: { method: "PUT" },
  expected: kFetchTestResult.failure,
}), "local to public: PUT preflight failure.");

subsetTestByKey("baseline", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: {
    port: kPorts.httpsPublic,
    searchParams: {
      "preflight-uuid": token(),
      "preflight-headers": "cors",
      "final-headers": "cors",
    },
  },
  fetchOptions: { method: "PUT" },
  expected: kFetchTestResult.success,
}), "local to public: PUT preflight success,");

// Generates tests of preflight behavior for a single (source, target) pair.
//
// Scenarios:
//
//  - cors mode:
//    - preflight response has non-2xx HTTP code
//    - preflight response is missing CORS headers
//    - preflight response is missing the PNA-specific `Access-Control` header
//    - final response is missing CORS headers
//    - success
//    - success with PUT method (non-"simple" request)
//  - no-cors mode:
//    - preflight response has non-2xx HTTP code
//    - preflight response is missing CORS headers
//    - preflight response is missing the PNA-specific `Access-Control` header
//    - success
//
function makePreflightTests({
  subsetKey,
  source,
  sourceDescription,
  targetPort,
  targetDescription,
}) {
  const prefix =
      `${sourceDescription} to ${targetDescription}: `;

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        // Missing "preflight-uuid" param: preflight will fail.
        "preflight-headers": "cors+pna",
        "final-headers": "cors",
      },
    },
    expected: kFetchTestResult.failure,
  }), prefix + "failed preflight.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
      },
    },
    expected: kFetchTestResult.failure,
  }), prefix + "missing CORS headers on preflight response.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors",
      },
    },
    expected: kFetchTestResult.failure,
  }), prefix + "missing PNA header on preflight response.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors+pna",
      },
    },
    expected: kFetchTestResult.failure,
  }), prefix + "missing CORS headers on final response.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors+pna",
        "final-headers": "cors",
      },
    },
    expected: kFetchTestResult.success,
  }), prefix + "success.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors+pna",
        "final-headers": "cors",
      },
    },
    fetchOptions: { method: "PUT" },
    expected: kFetchTestResult.success,
  }), prefix + "PUT success.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: { port: targetPort },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.failure,
  }), prefix + "no-CORS mode failed preflight.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: { "preflight-uuid": token() },
    },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.failure,
  }), prefix + "no-CORS mode missing CORS headers on preflight response.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors",
      },
    },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.failure,
  }), prefix + "no-CORS mode missing PNA header on preflight response.");

  subsetTestByKey(subsetKey, promise_test, t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
        "preflight-headers": "cors+pna",
      },
    },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.opaque,
  }), prefix + "no-CORS mode success.");
}

// Source: private secure context.
//
// Fetches to the local address space require a successful preflight response
// carrying a PNA-specific header.

makePreflightTests({
  subsetKey: "from-private",
  source: { port: kPorts.httpsPrivate },
  sourceDescription: "private",
  targetPort: kPorts.httpsLocal,
  targetDescription: "local",
});

subsetTestByKey("from-private", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "private to private: no preflight required.");

subsetTestByKey("from-private", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: {
    port: kPorts.httpsPublic,
    searchParams: { "final-headers": "cors" },
  },
  expected: kFetchTestResult.success,
}), "private to public: no preflight required.");

// Source: public secure context.
//
// Fetches to the local and private address spaces require a successful
// preflight response carrying a PNA-specific header.

makePreflightTests({
  subsetKey: "from-public",
  source: { port: kPorts.httpsPublic },
  sourceDescription: "public",
  targetPort: kPorts.httpsLocal,
  targetDescription: "local",
});

makePreflightTests({
  subsetKey: "from-public",
  source: { port: kPorts.httpsPublic },
  sourceDescription: "public",
  targetPort: kPorts.httpsPrivate,
  targetDescription: "private",
});

subsetTestByKey("from-public", promise_test, t => fetchTest(t, {
  source: { port: kPorts.httpsPublic },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "public to public: no preflight required.");

// These tests verify that documents fetched from the `local` address space yet
// carrying the `treat-as-public-address` CSP directive are treated as if they
// had been fetched from the `public` address space.

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.failure,
}), "treat-as-public-address to local: failed preflight.");

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: {
    port: kPorts.httpsLocal,
    searchParams: {
      "preflight-uuid": token(),
      "preflight-headers": "cors+pna",
      // Interesting: no need for CORS headers on same-origin final response.
    },
  },
  expected: kFetchTestResult.success,
}), "treat-as-public-address to local: success.");

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.failure,
}), "treat-as-public-address to private: failed preflight.");

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: {
    port: kPorts.httpsPrivate,
    searchParams: {
      "preflight-uuid": token(),
      "preflight-headers": "cors+pna",
      "final-headers": "cors",
    },
  },
  expected: kFetchTestResult.success,
}), "treat-as-public-address to private: success.");

subsetTestByKey("from-treat-as-public", promise_test, t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: {
    port: kPorts.httpsPublic,
    searchParams: {
      "final-headers": "cors",
    }
  },
  expected: kFetchTestResult.success,
}), "treat-as-public-address to public: no preflight required.");
