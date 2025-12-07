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
      Verify that generate-id() does not return the same value for different
      nodes.
    </p>
    <xsl:for-each select="/root/value">
      <xsl:variable name="current-node" select="." />
      <xsl:for-each select="./following-sibling::*">
        <xsl:variable name="subsequent-node" select="." />
        <xsl:variable name="first-id" select="generate-id($current-node)" />
        <xsl:variable name="second-id" select="generate-id($subsequent-node)" />
        <p>
          <xsl:value-of select="$current-node" /> and <xsl:value-of select="$subsequent-node" />
          <xsl:choose>
            <xsl:when test="$first-id != $second-id"> have distinct IDs as expected: PASSED</xsl:when>
            <xsl:otherwise> have matching IDs but are distinct nodes (got <xsl:value-of select="$first-id" /> and <xsl:value-of select="$second-id" />): FAILED</xsl:otherwise>
          </xsl:choose>
        </p>
      </xsl:for-each>
    </xsl:for-each>
    </body></html>
  </xsl:template>
</xsl:stylesheet>
