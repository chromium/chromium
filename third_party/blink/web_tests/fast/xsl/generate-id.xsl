<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html" version="1.0" encoding="UTF-8" />
  <xsl:template match="root">
    <html><body>
    <script>
      if (testRunner) {
        testRunner.dumpAsText();
      }
    </script>
    <p>
      Verify that generate-id() returns the same value for a given node in the
      current document in the current transform.
    </p>
    <xsl:for-each select="/root/value">
      <p>Value <xsl:value-of select="."/> IDs:
        <xsl:choose>
          <xsl:when test="generate-id() = generate-id()">matched!</xsl:when>
          <xsl:otherwise>didn't match!</xsl:otherwise>
        </xsl:choose>
      </p>
    </xsl:for-each>
    </body></html>
  </xsl:template>
</xsl:stylesheet>
