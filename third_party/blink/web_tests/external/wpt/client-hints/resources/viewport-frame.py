def main(request, response):
    """
    postMessage Viewport-Width headers
    """

    if b"viewport-width" in request.headers:
        result = request.headers["viewport-width"]
    else:
        result = u"FAIL"

    headers = [(b"Content-Type", b"text/html"),
               (b"Access-Control-Allow-Origin", b"*")]
    content = b'''
<script>
  let parentOrOpener = window.opener || window.parent;
  parentOrOpener.postMessage({ viewport: '%s' }, "*");
</script>
''' % (result)
    return 200, headers, content
