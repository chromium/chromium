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
        // The page is expected to have 'oversized-images' threshold
        // set to 2.0, i.e. images with actual_size / display_size ratio
        // > 2.0 should be replaced with placeholder images. A violation
        // report is also expected to be generated.

        async_test(t => {
          new ReportingObserver(t.step_func_done((reports, _) => {
            assert_equals(reports.length, 1);
            const report = reports[0];
            assert_equals(report.type, 'document-policy-violation');
            assert_equals(report.body.featureId, 'oversized-images');
            assert_equals(report.body.disposition, 'enforce');
          }), {types: ['document-policy-violation']}).observe();
        });

      </script>
    </head>
    <body bgcolor="#ffffff">
      <img src="resources/green-256x256.jpg" width="100"></img>
    </body>
  </html>
</xsl:template>

</xsl:stylesheet>