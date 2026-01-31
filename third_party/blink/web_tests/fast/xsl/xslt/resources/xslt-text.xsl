<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:output method="text" encoding="KOI8-R"/>
<xsl:template match="TEST">CHARACTERS IN XSLT: &lt;&lt;&lt;&amp;тест&amp;>>&gt;
<xsl:apply-templates/><xsl:text>&#10;</xsl:text></xsl:template>

</xsl:stylesheet>
