import re


def main(request, response):
    """Store the given beacon's data keyed by uuid in the server.

  For GET request, this handler assumes everything comes after 'data=' is part
  of the data query.
    """
    if b'uuid' not in request.GET:
        response.status = 400
        return 'Must provide a UUID to store beacon data'
    uuid = request.GET.first(b'uuid')
    if b'multipart/form-data' in request.headers.get(b'Content-Type', b''):
        data = request.POST.first(b'payload')
    elif request.body:
        data = request.body
    else:
        data = '<NO-DATA>'

    with request.server.stash.lock:
        request.server.stash.put(key=uuid, value=data, path='beacondata')
    return ((200, "OK"), [], "")
