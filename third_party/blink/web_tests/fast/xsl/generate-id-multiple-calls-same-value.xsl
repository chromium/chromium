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
      Verify that generate-id() returns the same value each time it is called
      for a given node in the current document in the current transform.
    </p>
    <xsl:for-each select="/root/value">
      <p>Value <xsl:value-of select="."/> IDs
        <xsl:variable name="first" select="generate-id()" />
        <xsl:variable name="second" select="generate-id()" />
        <xsl:choose>
          <xsl:when test="$first = $second">matched as expected: PASSED</xsl:when>
          <xsl:otherwise>didn't match (got <xsl:value-of select="$first" /> and <xsl:value-of select="$second" />): FAILED</xsl:otherwise>
        </xsl:choose>
      </p>
    </xsl:for-each>
    </body></html>
  </xsl:template>
</xsl:stylesheet>
