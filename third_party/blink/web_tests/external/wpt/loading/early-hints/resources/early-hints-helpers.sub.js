"use strict";

/**
 * Navigate to a test page with an Early Hints response.
 *
 * @typedef {Object} Preload
 * @property {string} url - A URL to preload. Note: This is relative to the
 *     `test_url` parameter of `navigateToTestWithEarlyHints()`.
 * @property {string} as_attr - `as` attribute of this preload.
 *
 * @param {string} test_url - URL of a test after the Early Hints response.
 * @param {Array<Preload>} preloads  - Preloads included in the Early Hints response.
 */
function navigateToTestWithEarlyHints(test_url, preloads) {
    const params = new URLSearchParams();
    params.set("test_url", test_url);
    for (const preload of preloads) {
        params.append("preloads", JSON.stringify(preload));
    }
    const url = "resources/early-hints-test-loader.h2.py?" + params.toString();
    window.location.replace(new URL(url, window.location));
}

/**
 * Parses the query string of the current window location and returns preloads
 * in the Early Hints response sent via `navigateToTestWithEarlyHints()`.
 *
 * @returns {Array<Preload>}
 */
function getPreloadsFromSearchParams() {
    const params = new URLSearchParams(window.location.search);
    const encoded_preloads = params.getAll("preloads");
    const preloads = [];
    for (const encoded of encoded_preloads) {
        preloads.push(JSON.parse(encoded));
    }
    return preloads;
}

/**
 * Navigate to the referrer policy test page.
 *
 * @param {string} referrer_policy - A value of Referrer-Policy to test.
 */
function testReferrerPolicy(referrer_policy) {
    const params = new URLSearchParams();
    params.set("referrer-policy", referrer_policy);
    const same_origin_preload_url = "https://{{host}}:{{ports[h2][0]}}/loading/early-hints/resources/fetch-and-record-js.h2.py?id=" + token();
    params.set("same-origin-preload-url", same_origin_preload_url);
    const cross_origin_preload_url = "https://{{hosts[alt][www]}}:{{ports[h2][0]}}/loading/early-hints/resources/fetch-and-record-js.h2.py?id=" + token();
    params.set("cross-origin-preload-url", cross_origin_preload_url);

    const path = "resources/referrer-policy-test-loader.h2.py?" + params.toString();
    const url = new URL(path, window.location);
    window.location.replace(url);
}
