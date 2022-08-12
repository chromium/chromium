# Increment the count of beacons the server has seen.
def main(request, response):
    if b"uuid" in request.GET:
        uuid = request.GET.first(b"uuid")
    else:
        response.status = 400
        return "Must provide a UUID for beacon count handlers"
    count = request.server.stash.take(key=uuid, path="beaconcount")
    if count is None:
        count = 0
    count += 1
    request.server.stash.put(key=uuid, value=count, path="beaconcount")
    return ((200, "OK"), [], "")
