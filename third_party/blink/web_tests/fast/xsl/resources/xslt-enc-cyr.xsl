<?xml version="1.0" encoding="iso8859-5"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html" encoding="KOI8-R"/>
    <xsl:template match="TEST">
        <html xmlns="http://www.w3.org/1999/xhtml">
            <body>
              CHARACTERS IN XSLT: Добавленный текст <br/> <xsl:apply-templates/>
            </body>
        </html>
  </xsl:template>
</xsl:stylesheet>