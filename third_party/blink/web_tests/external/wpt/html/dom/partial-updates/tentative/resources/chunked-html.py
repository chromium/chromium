import time


def main(request, response):
    delay = float(request.GET.first(b"delay", 300)) / 1000.0
    content1 = request.GET.first(b"chunk1", b"")
    content2 = request.GET.first(b"chunk2", b"")

    response.headers.set(b"Content-Type", b"text/html")
    if request.GET.first(b"cors", b"0") == b"1":
        response.headers.set(b"Access-Control-Allow-Origin", b"*")
    response.write_status_headers()
    response.writer.write_content(content1)
    time.sleep(delay)
    response.writer.write_content(content2)
