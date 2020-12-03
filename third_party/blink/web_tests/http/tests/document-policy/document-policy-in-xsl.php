<?php
header("Document-Policy: oversized-images=2.0");
header("Content-Type: application/xml");

echo '<?xml version="1.0"?>
<?xml-stylesheet href="resources/document-policy-in-xsl.xslt" type="text/xsl"?>
<page>
</page>';
?>
