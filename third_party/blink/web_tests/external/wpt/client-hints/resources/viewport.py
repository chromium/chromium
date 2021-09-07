def main(request, response):
    """
    Reflect Viewport-Width and Sec-Ch-Viewport-Height headers
    """

    if b"viewport-width" in request.headers and b"sec-ch-viewport-height" in request.headers:
        result = request.headers["viewport-width"] + b"," + request.headers["sec-ch-viewport-height"]
    else:
        result = u"FAIL"

    headers = [(b"Content-Type", b"text/html"),
               (b"Access-Control-Allow-Origin", b"*")]
    return 200, headers, result
