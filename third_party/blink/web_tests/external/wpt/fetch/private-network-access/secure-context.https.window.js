// META: script=/common/utils.js
// META: script=resources/support.js
// META: script=resources/ports.sub.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// This file covers only those tests that must execute in a secure context.
// Other tests are defined in: non-secure-context.window.js

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

// These tests verify that secure contexts can fetch subresources from all
// address spaces.

// Source: secure local context.
//
// All fetches unaffected by Private Network Access.

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "local to local: no preflight required.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: {
    port: kPorts.httpsPrivate,
    searchParams: { "final-headers": "cors" },
  },
  expected: kFetchTestResult.success,
}), "local to private: no preflight required.");


promise_test(t => fetchTest(t, {
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

promise_test(t => fetchTest(t, {
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

promise_test(t => fetchTest(t, {
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

// Source: private secure context.
//
// Fetches to the local address space require a successful preflight response
// carrying a PNA-specific header.

function makePreflightTests({
  source,
  sourceDescription,
  targetPort,
  targetDescription,
}) {
  const prefix =
      `${sourceDescription} to ${targetDescription}: `;

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: {
        "preflight-uuid": token(),
      },
    },
    expected: kFetchTestResult.failure,
  }), prefix + "missing CORS headers on preflight response.");

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
    source,
    target: { port: targetPort },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.failure,
  }), prefix + "no-CORS mode failed preflight.");

  promise_test(t => fetchTest(t, {
    source,
    target: {
      port: targetPort,
      searchParams: { "preflight-uuid": token() },
    },
    fetchOptions: { mode: "no-cors" },
    expected: kFetchTestResult.failure,
  }), prefix + "no-CORS mode missing CORS headers on preflight response.");

  promise_test(t => fetchTest(t, {
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

  promise_test(t => fetchTest(t, {
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

makePreflightTests({
  source: { port: kPorts.httpsPrivate },
  sourceDescription: "private",
  targetPort: kPorts.httpsLocal,
  targetDescription: "local",
});

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "private to private: no preflight required.");

promise_test(t => fetchTest(t, {
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
  source: { port: kPorts.httpsPublic },
  sourceDescription: "public",
  targetPort: kPorts.httpsLocal,
  targetDescription: "local",
});

makePreflightTests({
  source: { port: kPorts.httpsPublic },
  sourceDescription: "public",
  targetPort: kPorts.httpsPrivate,
  targetDescription: "private",
});

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPublic },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "public to public: no preflight required.");

// These tests verify that documents fetched from the `local` address space yet
// carrying the `treat-as-public-address` CSP directive are treated as if they
// had been fetched from the `public` address space.

promise_test(t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.failure,
}), "treat-as-public-address to local: failed preflight.");

promise_test(t => fetchTest(t, {
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

promise_test(t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    headers: { "Content-Security-Policy": "treat-as-public-address" },
  },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.failure,
}), "treat-as-public-address to private: failed preflight.");

promise_test(t => fetchTest(t, {
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

promise_test(t => fetchTest(t, {
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

// These tests verify that websocket connections behave similarly to fetches.

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsLocal,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "local to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsPrivate,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "private to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsPublic,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "public to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsLocal,
    treatAsPublicAddress: true,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "treat-as-public to local: websocket success.");
