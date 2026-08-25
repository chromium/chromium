// META: spec=https://w3c.github.io/payment-method-manifest/#link-http-header-required
// META: title=Payment Method Manifest fetch fails if Link header is missing
// META: script=/common/utils.js
// META: script=/payment-method-manifest/resources/helpers.js

promise_test(async t => {
  const testId = token();
  const pmiUrl = createPaymentMethodIdentifierUrl(testId, { link: 'none' });

  const request = new PaymentRequest(
    [{ supportedMethods: pmiUrl }],
    { total: { label: 'Total', amount: { currency: 'USD', value: '1.00' } } }
  );

  try {
    await request.canMakePayment();
  } catch (err) {}

  // Expecting only 1 request (HEAD to PMI). Since no Link header is returned,
  // manifest GET must NOT be performed.
  const logs = await waitForServerAccessLogs(t, testId, 1);

  assert_equals(logs.length, 1, 'Browser must issue only 1 server request (HEAD to PMI)');
  assert_equals(logs[0].endpoint, 'payment-method-identifier', 'Request must hit PMI URL');
  assert_equals(logs[0].method, 'HEAD', 'PMI request must use HEAD method');
}, 'Payment Method Manifest fetch fails if Link header is missing');
