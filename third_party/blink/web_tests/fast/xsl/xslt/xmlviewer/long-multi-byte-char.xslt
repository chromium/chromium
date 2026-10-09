<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"  version="1.0" xmlns:v="urn:schemas-microsoft-com:vml">
  <xsl:output method="html" encoding="utf-8" doctype-system="about:legacy-compat" />

  <xsl:template match="/">
    <html xmlns:v="urn:schemas-microsoft-com:vml" class="reftest-wait">
      <head>
        <script>
          // Remove the XSLT deprecation banner, which would cover the text.
          // The banner is inserted after the transformed document is
          // committed, so wait for a couple of frames.
          requestAnimationFrame(() => {
            requestAnimationFrame(() => {
              document.querySelector('xslt-warning-banner')?.remove();
              document.documentElement.classList.remove('reftest-wait');
            });
          });
        </script>
      </head>
      <body id="test-body">
        Test passes if you see this line, and this line alone.
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
