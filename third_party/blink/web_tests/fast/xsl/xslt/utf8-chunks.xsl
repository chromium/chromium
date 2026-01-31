<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="xml" indent="yes" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" 
              doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"/>

  <xsl:template match="/">
    <html>
      <body>
        <script>
          if (window.testRunner)
            testRunner.dumpAsText();
        </script>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

</xsl:stylesheet>
