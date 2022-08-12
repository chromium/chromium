"""An HTTP request handler for WPT that handles /set_beacon.py requests."""

_BEACON_ID_KEY = b"uuid"
_BEACON_DATA_PATH = "beacon_data"
_BEACON_FORM_PAYLOAD_KEY = b"payload"
_BEACON_BODY_PAYLOAD_KEY = "payload="


def main(request, response):
    """Stores the given beacon's data keyed by uuid in the server.

    For GET request, this handler assumes no data.
    For POST request, this handler extracts data from request body:
      - Content-Type=multipart/form-data: data keyed by 'payload'.
      - the entire request body.

    Multiple data can be added for the same uuid.

    The data is stored as UTF-8 format.
    """
    if _BEACON_ID_KEY not in request.GET:
        response.status = 400
        return "Must provide a UUID to store beacon data"
    uuid = request.GET.first(_BEACON_ID_KEY)

    data = None
    if request.method == u"POST":
        if b"multipart/form-data" in request.headers.get(b"Content-Type", b""):
            if _BEACON_FORM_PAYLOAD_KEY in request.POST:
                data = request.POST.first(_BEACON_FORM_PAYLOAD_KEY).decode(
                    'utf-8')
        elif request.body:
            data = request.body.decode('utf-8')
            if data.startswith(_BEACON_BODY_PAYLOAD_KEY):
                data = data.split(_BEACON_BODY_PAYLOAD_KEY)[1]

    with request.server.stash.lock:
        saved_data = request.server.stash.take(key=uuid, path=_BEACON_DATA_PATH)
        if not saved_data:
            saved_data = [data]
        else:
            saved_data.append(data)
        request.server.stash.put(
            key=uuid, value=saved_data, path=_BEACON_DATA_PATH)
    return ((200, "OK"), [], "")
