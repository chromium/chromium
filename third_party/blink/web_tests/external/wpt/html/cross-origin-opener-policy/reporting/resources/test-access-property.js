const directory = "/html/cross-origin-opener-policy/reporting";
const executor_path = directory + "/resources/executor.html?pipe=";
const coep_header = '|header(Cross-Origin-Embedder-Policy,require-corp)';

const origin = [
  ["cross-origin", get_host_info().HTTPS_REMOTE_ORIGIN ] ,
  ["same-site"   , get_host_info().HTTPS_ORIGIN        ] ,
];

let testAccessProperty = (property, op, message) => {
  origin.forEach(([origin_name, origin]) => {
    promise_test(async t => {
      const report_token = token();
      const openee_token = token();
      const opener_token = token(); // The current test window.

      const reportTo = reportToHeaders(report_token);
      const openee_url = origin + executor_path + reportTo.header +
        reportTo.coopReportOnlySameOriginHeader + coep_header +
        `&uuid=${openee_token}`;
      const openee = window.open(openee_url);
      t.add_cleanup(() => send(openee_token, "window.close()"))

      // 1. Make sure the new document to be loaded.
      send(openee_token, `send("${opener_token}", "Ready");`);
      assert_equals(await receive(opener_token), "Ready");
      // TODO(arthursozogni): Figure out why 2 round-trip is sometimes
      // necessary to ensure the CoopAccessMonitor are installed.
      send(openee_token, `send("${opener_token}", "Ready");`);
      assert_equals(await receive(opener_token), "Ready");

      // 2. Try to access the openee. This shouldn't work because of COOP+COEP.
      try {op(openee)} catch(e) {}

      // 3. Check a reports is sent to the opener.
      let report = await receiveReport(report_token,
        "access-to-coop-page-from-opener");
      assert_equals(report.body.property, property);

    }, `${origin_name} > ${op}`);
  })
};
