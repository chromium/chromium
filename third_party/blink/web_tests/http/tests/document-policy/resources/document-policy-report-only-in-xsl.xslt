<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="page">
  <html>
    <head>
      <title> Test XSLT </title>
      <script src="../../resources/testharness.js"></script>
      <script src="../../resources/testharnessreport.js"></script>
      <script>
        const check_report_format = ([reports, _]) => {
          assert_equals(reports.length, 1);
          const report = reports[0];
          assert_equals(report.type, 'document-policy-violation');
          assert_equals(report.body.featureId, 'sync-xhr');
          assert_equals(report.body.disposition, 'report');
        };

        promise_test(async t => {
          const report = new Promise(resolve => {
            new ReportingObserver((reports, observer) => resolve([reports, observer]),
                                  {types: ['document-policy-violation']}).observe();
          });
          const xhr = new XMLHttpRequest();
          xhr.open("GET", document.location.href, false);
          xhr.send();
          check_report_format(await report);
        });
      </script>
    </head>
    <body bgcolor="#ffffff">
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>
