<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html" encoding="UTF-16"/>
    <xsl:template match="TEST">
        <html xmlns="http://www.w3.org/1999/xhtml">
            <body>
              CHARACTERS IN XSLT: ééééééééééé <br/> <xsl:apply-templates/>
            </body>
        </html>
  </xsl:template>
</xsl:stylesheet>