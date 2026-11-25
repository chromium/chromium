def main(request, response):
    headers = [(b"Cache-Control", b"no-store")]

    if b"css" in request.GET:
        headers.append((b"Content-Type", b"text/css"))
        if b"cors" in request.GET:
            headers.append((b"Access-Control-Allow-Origin", b"*"))
        return 200, headers, b":root { color: rgb(0, 128, 0) }"

    link = b"<" + request.GET.first(b"href") + b">; rel=stylesheet"
    if b"crossorigin" in request.GET:
        value = request.GET.first(b"crossorigin")
        link += b"; crossorigin=" + value if value else b"; crossorigin"
    headers.append((b"Link", link))
    headers.append((b"Content-Type", b"text/html"))
    return 200, headers, b"<!doctype html>"
