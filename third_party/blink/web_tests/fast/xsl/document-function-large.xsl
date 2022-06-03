<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:variable name="items" select="document('resources/larger-than-64kb-document.xml')/doc/listitem"/>
<xsl:template match="/">
  <html>
  <script>
    if (window.testRunner) testRunner.dumpAsText();
    if (5 == <xsl:value-of select="count($items)"/>)
        document.write("PASS: ");
    else
        document.write("FAIL, expected 5 got: ");
  </script>
  <xsl:value-of select="count($items)"/> items.
  </html>
</xsl:template>
</xsl:stylesheet>