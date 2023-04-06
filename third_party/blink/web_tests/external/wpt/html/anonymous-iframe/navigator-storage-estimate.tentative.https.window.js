// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=/html/cross-origin-embedder-policy/credentialless/resources/common.js
// META: script=./resources/common.js

promise_test(async test => {
  // 4 actors: 2 normal iframes and 2 credentialless iframes.
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const iframes = [
    newIframe(origin),
    newIframe(origin),
    newIframeCredentialless(origin),
    newIframeCredentialless(origin),
  ];

  // "token()" is used to get unique value for every execution of the test. This
  // avoids potential side effects of one run toward the second.
  const g_db_store = token();
  const g_db_name = token();
  const g_db_version = 1;

  // 1. Write a different key-value pair from every iframe in IndexedDB. To
  //    check usage is accounted separately, values of different sizes are used.
  await Promise.all(iframes.map(async (iframe, i) => {
    const response_channel = token();
    send(iframe, `
      // Open the database:
      const request = indexedDB.open("${g_db_name}", "${g_db_version}");
      request.onupgradeneeded = () => {
        request.result.createObjectStore("${g_db_store}", {keyPath: "id"});
      };
      await new Promise(r => request.onsuccess = r);
      const db = request.result;

      // Write the value:
      const transaction_write = db.transaction("${g_db_store}", "readwrite");
      transaction_write.objectStore("${g_db_store}").add({
        id: "${token()}",
        value: new Uint8Array(${Math.pow(10, 2+i)})
      });
      await transaction_write.complete;

      db.close();
      send("${response_channel}", "Done");
    `);

    assert_equals(await receive(response_channel), "Done");
  }));

  // 2. Read the quota estimation for every iframes.
  const estimates = await Promise.all(iframes.map(async iframe => {
    const response_channel = token();
    send(iframe, `
      const {quota, usage} = await navigator.storage.estimate();
      send("${response_channel}", JSON.stringify({quota, usage}));
    `);
    return JSON.parse(await receive(response_channel));
  }));

  // Two storage bucket must be used:
  // - One for the two credentialless iframes,
  // - One for the two normal iframes.
  assert_equals(estimates[0].usage, estimates[1].usage,
    "Normal iframes must share the same bucket");
  assert_equals(estimates[2].usage, estimates[3].usage,
    "Credentialless iframes must share the same storage bucket");
  assert_greater_than(estimates[2].usage, 10 * estimates[0].usage,
    "Normal and credentialless iframes must not not share the same bucket");

  // A priori, buckets for credentiallesses and normal iframes have no reasons
  // to be assigned different capacities:
  assert_equals(estimates[0].quota, estimates[1].quota, "Same quota (0 vs 1)");
  assert_equals(estimates[0].quota, estimates[2].quota, "Same quota (0 vs 2)");
  assert_equals(estimates[0].quota, estimates[3].quota, "Same quota (0 vs 3)");
})
