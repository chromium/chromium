import datetime
import json


def _url_dir(request):
    return u'/'.join(request.url_parts.path.split(u'/')[:-1]) + u'/'


def store_request_timing_and_headers(request):
    """Store the current timestamp and request's headers in the stash object of
    the server. The request must a GET request and must have the "id" parameter.
    """
    id = request.GET.first(b"id")
    timestamp = datetime.datetime.now().timestamp()

    value = {
        "timestamp": timestamp,
        "headers": request.raw_headers,
    }

    url_dir = _url_dir(request)
    request.server.stash.put(id, value, url_dir)


def get_request_timing_and_headers(request):
    """Get previously stored timestamp and request headers associated with the
    given request. The request must be a GET request and must have the "id"
    parameter.
    """
    id = request.GET.first(b"id")
    url_dir = _url_dir(request)
    item = request.server.stash.take(id, url_dir)
    return json.dumps(item)
