# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from six.moves.urllib import parse
from mod_pywebsocket import stream
import threading


cv = threading.Condition()
connected = False
close_code = None


def get_role(request):
  """Look up the "role" query parameter in the URL."""
  query = request.ws_resource.split('?', 1)
  if len(query) == 1:
    raise LookupError('No query string found in URL')
  param = parse.parse_qs(query[1])
  if 'role' not in param:
    raise LookupError('No role parameter found in the query string')
  return param['role'][0]


def be_observed(request):
  global connected
  with cv:
    connected = True
  # Wait for a Close frame
  try:
    request.ws_stream.receive_message()
  except stream.ConnectionTerminatedException:
    observe_close(1006)  # "Abnormal Closure"


def be_observer(request):
  with cv:
    if not connected:
      request.ws_stream.send_message('NOT CONNECTED', binary = False)
    else:
      while close_code is None:
        cv.wait()
      if close_code == 1001:  # "Going Away"
        request.ws_stream.send_message('OK', binary = False)
      else:
        request.ws_stream.send_message('WRONG CODE %d' % close_code,
                                       binary = False)
  request.ws_stream.close_connection()


def web_socket_do_extra_handshake(request):
  pass


def web_socket_transfer_data(request):
  role = get_role(request)
  if role == 'observed':
    be_observed(request)
  elif role == 'observer':
    be_observer(request)
  else:
    raise ValueError('Bad role "%s"' % role)


def web_socket_passive_closing_handshake(request):
  if get_role(request) == 'observed':
    observe_close(request.ws_close_code)
  return request.ws_close_code, request.ws_close_reason


def observe_close(code):
  global close_code
  with cv:
    close_code = code
    cv.notify()
