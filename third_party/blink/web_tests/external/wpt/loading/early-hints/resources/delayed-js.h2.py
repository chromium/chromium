import time


def main(request, response):
    id = request.GET.first(b"id")
    url_dir = u'/'.join(request.url_parts.path.split(u'/')[:-1]) + u'/'
    # Wait until the id is set via resume-delayed-js.h2.py.
    while True:
        if request.server.stash.take(id, url_dir):
            break
        time.sleep(0.1)

    headers = [
        ("Content-Type", "text/javascript"),
        ("Cache-Control", "max-age=600"),
    ]
    body = "/*empty script*/"
    return (200, "OK"), headers, body
