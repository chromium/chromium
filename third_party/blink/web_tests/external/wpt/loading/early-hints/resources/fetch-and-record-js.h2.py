import importlib

utils = importlib.import_module("loading.early-hints.resources.utils")


def main(request, response):
    utils.store_request_timing_and_headers(request)
    headers = [
        ("Content-Type", "text/javascript"),
        ("Cache-Control", "max-age=600"),
    ]
    body = "/*empty script*/"
    return (200, "OK"), headers, body
