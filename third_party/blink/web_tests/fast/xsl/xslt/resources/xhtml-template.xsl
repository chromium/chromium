<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xhtml="http://www.w3.org/1999/xhtml"
    xmlns="http://www.w3.org/1999/xhtml"
    xmlns:xs="http://www.w3.org/2001/XMLSchema"
    exclude-result-prefixes="xhtml xsl xs">
    <xsl:output method="xml" omit-xml-declaration="yes"/>
    <xsl:template match="xhtml:html">
        <html>
            <body>
                <template>
                    <xsl:apply-templates select="//xhtml:body/xhtml:span" />
                </template>
                <xsl:apply-templates select="//xhtml:template//xhtml:span" />
                <p>This tests that XSLT transforms can traverse into XHTML template element content when applying XSL template.
                   If the test succeeds, the transform will have swapped the position of the body spans (A and B) with the template content spans (C and D)
                   and replaced the spans with divs.</p>
<script>
if (window.testRunner)
    testRunner.dumpAsText();

function divChildTextNodes(parent) {
    var output = '';

    for (var child = parent.firstChild; child; child = child.nextSibling) {
        if (child.tagName == 'div') {
            output += child.textContent;
        }
    }

    return output;
}

var span = document.body.appendChild(document.createElement('span'));
span.textContent = 'Body divs: ' + divChildTextNodes(document.body);

span = document.body.appendChild(document.createElement('span'));
var template = document.querySelector('template');
span.textContent = ', Template content divs: ' + divChildTextNodes(template.content);
</script>
            </body>
        </html>
    </xsl:template>
    <xsl:template match="xhtml:span">
        <div><xsl:value-of select="text()" /></div>
    </xsl:template>
</xsl:stylesheet>