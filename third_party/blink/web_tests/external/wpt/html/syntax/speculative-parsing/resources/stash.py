def main(request, response):
    if request.GET[b"action"] == b"put":
        encodingcheck = u"param-encodingcheck: " + request.url_parts.query.split(u"&encodingcheck=")[1] + u"\r\n"
        request.server.stash.put(request.GET[b"uuid"], encodingcheck + str(request.raw_headers))
        return u''
    return request.server.stash.take(request.GET[b"uuid"])
