#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launch a local http server, then launch a executable directed at the server.

This command creates a local server (on port 5103 by default) then runs:
  <executable> <args..> http://localhost:<port>/<page>.

Where <page> can be set by -P, or uses index.html by default.
"""

import argparse
import copy
import getos
import os
import subprocess
import sys
import httpd


if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-C', '--serve-dir',
      help='Serve files out of this directory.',
      dest='serve_dir', default=os.path.abspath('.'))
  parser.add_argument('-P', '--path', help='Path to load from local server.',
      dest='path', default='index.html')
  parser.add_argument('-D',
      help='Add debug command-line when launching the chrome debug.',
      dest='debug', action='append', default=[])
  parser.add_argument('-E',
      help='Add environment variables when launching the executable.',
      dest='environ', action='append', default=[])
  parser.add_argument('-p', '--port',
      help='Port to run server on. Default is 5103, ephemeral is 0.',
      type=int, default=5103)
  parser.add_argument('executable', help='command to run')
  parser.add_argument('args', nargs='*', help='arguments for executable')
  options = parser.parse_args(args)

  # 0 means use an ephemeral port.
  server = httpd.LocalHTTPServer(options.serve_dir, options.port)
  print 'Serving %s on %s...' % (options.serve_dir, server.GetURL(''))

  env = copy.copy(os.environ)
  for e in options.environ:
    key, value = map(str.strip, e.split('='))
    env[key] = value

  cmd = [options.executable] + options.args + [server.GetURL(options.path)]
  print 'Running: %s...' % (' '.join(cmd),)
  process = subprocess.Popen(cmd, env=env)

  # If any debug args are passed in, assume we want to debug
  if options.debug:
    if getos.GetPlatform() == 'linux':
      cmd = ['xterm', '-title', 'NaCl Debugger', '-e']
      cmd += options.debug
    elif getos.GetPlatform() == 'mac':
      cmd = ['osascript', '-e',
             'tell application "Terminal" to do script "%s"' %
                 ' '.join(r'\"%s\"' % x for x in options.debug)]
    elif getos.GetPlatform() == 'win':
      cmd = ['cmd.exe', '/c', 'start', 'cmd.exe', '/c']
      cmd += options.debug
    print 'Starting debugger: ' + ' '.join(cmd)
    debug_process = subprocess.Popen(cmd, env=env)
  else:
    debug_process = False

  try:
    return server.ServeUntilSubprocessDies(process)
  finally:
    if process.returncode is None:
      process.kill()
    if debug_process and debug_process.returncode is None:
      debug_process.kill()

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
