import os.path

WPT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../.."))

def main(request, response):
    cookie_name = request.GET.first(b"cookie")
    has_cookie = (cookie_name + b"=1") in request.headers.get(b"cookie", b"")
    passes = has_cookie == (request.GET.first(b"expect") == b"cookie")

    headers = [(b"cache-control", b"no-store")]
    if request.GET.first(b"acao") == b"origin":
        headers += [(b"access-control-allow-origin", request.headers.get(b"origin", b"")), (b"access-control-allow-credentials", b"true")]
    else:
        headers.append((b"access-control-allow-origin", b"*"))

    if request.GET.first(b"kind") == b"font":
        if not passes:
            return 404, headers, b""
        path, content_type = os.path.join(WPT_ROOT, "fonts", "Ahem.ttf"), b"font/ttf"
    else:
        path, content_type = os.path.join(WPT_ROOT, "css", "support", "1x1-green.png" if passes else "1x1-red.png"), b"image/png"

    headers.append((b"content-type", content_type))
    with open(path, "rb") as f:
        return headers, f.read()
