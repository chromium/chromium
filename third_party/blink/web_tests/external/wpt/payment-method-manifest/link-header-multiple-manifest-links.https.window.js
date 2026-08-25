// META: spec=https://w3c.github.io/payment-method-manifest/#fetch-pmm
// META: title=Multiple rel="payment-method-manifest" link headers cause fetch to abort
// META: script=/common/utils.js
// META: script=/payment-method-manifest/resources/helpers.js

promise_test(async t => {
  const testId = token();
  const manifestUrl1 = createPaymentMethodManifestUrl(testId);
  const manifestUrl2 = createPaymentMethodManifestUrl(testId);
  const pmiUrl = createPaymentMethodIdentifierUrl(testId, {
    link: [
      `<${manifestUrl1}>; rel="payment-method-manifest"`,
      `<${manifestUrl2}>; rel="payment-method-manifest"`
    ]
  });

  const request = new PaymentRequest(
    [{ supportedMethods: pmiUrl }],
    { total: { label: 'Total', amount: { currency: 'USD', value: '1.00' } } }
  );

  try {
    await request.canMakePayment();
  } catch (err) {}

  const logs = await waitForServerAccessLogs(t, testId, 1);

  assert_equals(logs.length, 1, 'Browser must issue only 1 server request (HEAD to PMI); duplicate manifest links abort fetch');
  assert_equals(logs[0].endpoint, 'payment-method-identifier', 'Request must hit PMI URL');
  assert_equals(logs[0].method, 'HEAD', 'PMI request must use HEAD method');
}, 'Multiple rel="payment-method-manifest" link headers cause fetch to abort');
