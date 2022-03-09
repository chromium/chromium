import importlib

utils = importlib.import_module("loading.early-hints.resources.utils")


def main(request, response):
    headers = [("Content-Type", "application/json")]
    body = utils.get_request_timing_and_headers(request)
    return (200, "OK"), headers, body
