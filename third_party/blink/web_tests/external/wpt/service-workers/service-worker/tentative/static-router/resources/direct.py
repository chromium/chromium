import os

def main(request, response):
    return 200, [(b'Content-Type', b'text/plain')], u'Network with %s request' % request.method
