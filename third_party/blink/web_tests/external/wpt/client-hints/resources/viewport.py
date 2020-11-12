def main(request, response):
    """
    Reflect Viewport-Width headers
    """

    if b"viewport-width" in request.headers:
        result = request.headers["viewport-width"]
    else:
        result = u"FAIL"

    headers = [(b"Content-Type", b"text/html"),
               (b"Access-Control-Allow-Origin", b"*")]
    return 200, headers, result
