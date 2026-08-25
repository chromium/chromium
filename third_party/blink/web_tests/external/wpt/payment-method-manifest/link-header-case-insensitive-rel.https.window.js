// META: spec=https://w3c.github.io/payment-method-manifest/#fetch-pmm
// META: title=Link header rel parameter matching is ASCII case-insensitive
// META: script=/common/utils.js
// META: script=/payment-method-manifest/resources/helpers.js

promise_test(async t => {
  const testId = token();
  const manifestUrl = createPaymentMethodManifestUrl(testId);
  const pmiUrl = createPaymentMethodIdentifierUrl(testId, { link: `<${manifestUrl}>; rel="PAYMENT-METHOD-MANIFEST"` });

  const request = new PaymentRequest(
    [{ supportedMethods: pmiUrl }],
    { total: { label: 'Total', amount: { currency: 'USD', value: '1.00' } } }
  );

  try {
    await request.canMakePayment();
  } catch (err) {}

  const logs = await waitForServerAccessLogs(t, testId, 2);

  assert_equals(logs.length, 2, 'Browser must issue exactly 2 server requests (HEAD for PMI, GET for Manifest)');
  assert_equals(logs[0].endpoint, 'payment-method-identifier', 'First request must hit PMI URL');
  assert_equals(logs[0].method, 'HEAD', 'PMI request must use HEAD method');
  assert_equals(logs[1].endpoint, 'payment-method-manifest', 'Second request must hit manifest URL');
  assert_equals(logs[1].method, 'GET', 'Manifest request must use GET method');
}, 'Link header rel parameter matching is ASCII case-insensitive');
