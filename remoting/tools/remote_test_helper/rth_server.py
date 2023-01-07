# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import jsonrpclib
import SimpleJSONRPCServer as _server


class RequestHandler(_server.SimpleJSONRPCRequestHandler):
  """Custom JSON-RPC request handler."""

  FILES = {
      'client.html': {'content-type': 'text/html'},
      'host.html': {'content-type': 'text/html'},
      'client.js': {'content-type': 'application/javascript'},
      'host.js': {'content-type': 'application/javascript'},
      'jsonrpc.js': {'content-type': 'application/javascript'}
      }

  def do_GET(self):
    """Custom GET handler to return default pages."""
    filename = self.path.lstrip('/')
    if filename not in self.FILES:
      self.report_404()
      return
    with open(filename) as f:
      data = f.read()
    self.send_response(200)
    for key, value in self.FILES[filename].iteritems():
      self.send_header(key, value)
    self.end_headers()
    self.wfile.write(data)


class RPCHandler(object):
  """Class to define and handle RPC calls."""

  CLEARED_EVENT = {'action': 0, 'event': 0, 'modifiers': 0}

  def __init__(self):
    self.last_event = self.CLEARED_EVENT

  def ClearLastEvent(self):
    """Clear the last event."""
    self.last_event = self.CLEARED_EVENT
    return True

  def SetLastEvent(self, action, value, modifier):
    """Set the last action, value, and modifiers."""
    self.last_event = {
        'action': action,
        'value': value,
        'modifiers': modifier
        }
    return True

  def GetLastEvent(self):
    return self.last_event


def main():
  server = _server.SimpleJSONRPCServer(
      ('', 3474), requestHandler=RequestHandler,
      logRequests=True, allow_none=True)
  server.register_instance(RPCHandler())
  server.serve_forever()


if __name__ == '__main__':
  main()
