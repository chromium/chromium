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
      Each pair of IDs below should match, as the same ID should be generated
      for the same node in the current document in the current transform.
    </p>
    <xsl:for-each select="/root/value">
      <p>Value <xsl:value-of select="."/> IDs:
        <xsl:value-of select="generate-id(.)"/>
        <xsl:value-of select="generate-id(.)"/>
      </p>
    </xsl:for-each>
    </body></html>
  </xsl:template>
</xsl:stylesheet>
