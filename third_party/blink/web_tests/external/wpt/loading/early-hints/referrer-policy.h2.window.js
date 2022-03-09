// META: script=/common/utils.js

function test_referrer_policy(referrer_policy) {
    const params = new URLSearchParams();
    const preload_url = "fetch-and-record-js.h2.py?id=" + token();
    params.set("preload-url", preload_url);
    params.set("referrer-policy", referrer_policy);
    const path = "resources/referrer-policy-test-loader.h2.py?" + params.toString();
    const url = new URL(path, window.location);
    window.location.replace(url);
}

// TODO(https://crbug.com/1302851): Add more test cases.
test(() => test_referrer_policy("no-referrer"));
