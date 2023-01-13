#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import optparse
import os.path
import socket
import sys
import thread
import time
import urllib

# Allow the import of third party modules
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(script_dir, '../../../../third_party/'))
sys.path.insert(0, os.path.join(script_dir, '../../../../tools/valgrind/'))
sys.path.insert(0, os.path.join(script_dir, '../../../../testing/'))

import browsertester.browserlauncher
import browsertester.rpclistener
import browsertester.server

import memcheck_analyze


def BuildArgParser():
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage)

  parser.add_option('-p', '--port', dest='port', action='store', type='int',
                    default='0', help='The TCP port the server will bind to. '
                    'The default is to pick an unused port number.')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Use the browser located here.')
  parser.add_option('--map_file', dest='map_files', action='append',
                    type='string', nargs=2, default=[],
                    metavar='DEST SRC',
                    help='Add file SRC to be served from the HTTP server, '
                    'to be made visible under the path DEST.')
  parser.add_option('--serving_dir', dest='serving_dirs', action='append',
                    type='string', default=[],
                    metavar='DIRNAME',
                    help='Add directory DIRNAME to be served from the HTTP '
                    'server to be made visible under the root.')
  parser.add_option('--output_dir', dest='output_dir', action='store',
                    type='string', default=None,
                    metavar='DIRNAME',
                    help='Set directory DIRNAME to be the output directory '
                    'when POSTing data to the server. NOTE: if this flag is '
                    'not set, POSTs will fail.')
  parser.add_option('--test_arg', dest='test_args', action='append',
                    type='string', nargs=2, default=[],
                    metavar='KEY VALUE',
                    help='Parameterize the test with a key/value pair.')
  parser.add_option('--redirect_url', dest='map_redirects', action='append',
                    type='string', nargs=2, default=[],
                    metavar='DEST SRC',
                    help='Add a redirect to the HTTP server, '
                    'requests for SRC will result in a redirect (302) to DEST.')
  parser.add_option('-f', '--file', dest='files', action='append',
                    type='string', default=[],
                    metavar='FILENAME',
                    help='Add a file to serve from the HTTP server, to be '
                    'made visible in the root directory.  '
                    '"--file path/to/foo.html" is equivalent to '
                    '"--map_file foo.html path/to/foo.html"')
  parser.add_option('--mime_type', dest='mime_types', action='append',
                    type='string', nargs=2, default=[], metavar='DEST SRC',
                    help='Map file extension SRC to MIME type DEST when '
                    'serving it from the HTTP server.')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None,
                    help='The webpage to load.')
  parser.add_option('--ppapi_plugin', dest='ppapi_plugin', action='store',
                    type='string', default=None,
                    help='Use the browser plugin located here.')
  parser.add_option('--ppapi_plugin_mimetype', dest='ppapi_plugin_mimetype',
                    action='store', type='string', default='application/x-nacl',
                    help='Associate this mimetype with the browser plugin. '
                    'Unused if --ppapi_plugin is not specified.')
  parser.add_option('--sel_ldr', dest='sel_ldr', action='store',
                    type='string', default=None,
                    help='Use the sel_ldr located here.')
  parser.add_option('--sel_ldr_bootstrap', dest='sel_ldr_bootstrap',
                    action='store', type='string', default=None,
                    help='Use the bootstrap loader located here.')
  parser.add_option('--irt_library', dest='irt_library', action='store',
                    type='string', default=None,
                    help='Use the integrated runtime (IRT) library '
                    'located here.')
  parser.add_option('--interactive', dest='interactive', action='store_true',
                    default=False, help='Do not quit after testing is done. '
                    'Handy for iterative development.  Disables timeout.')
  parser.add_option('--debug', dest='debug', action='store_true', default=False,
                    help='Request debugging output from browser.')
  parser.add_option('--timeout', dest='timeout', action='store', type='float',
                    default=5.0,
                    help='The maximum amount of time to wait, in seconds, for '
                    'the browser to make a request. The timer resets with each '
                    'request.')
  parser.add_option('--hard_timeout', dest='hard_timeout', action='store',
                    type='float', default=None,
                    help='The maximum amount of time to wait, in seconds, for '
                    'the entire test.  This will kill runaway tests. ')
  parser.add_option('--allow_404', dest='allow_404', action='store_true',
                    default=False,
                    help='Allow 404s to occur without failing the test.')
  parser.add_option('-b', '--bandwidth', dest='bandwidth', action='store',
                    type='float', default='0.0',
                    help='The amount of bandwidth (megabits / second) to '
                    'simulate between the client and the server. This used for '
                    'replies with file payloads. All other responses are '
                    'assumed to be short. Bandwidth values <= 0.0 are assumed '
                    'to mean infinite bandwidth.')
  parser.add_option('--extension', dest='browser_extensions', action='append',
                    type='string', default=[],
                    help='Load the browser extensions located at the list of '
                    'paths. Note: this currently only works with the Chrome '
                    'browser.')
  parser.add_option('--tool', dest='tool', action='store',
                    type='string', default=None,
                    help='Run tests under a tool.')
  parser.add_option('--browser_flag', dest='browser_flags', action='append',
                    type='string', default=[],
                    help='Additional flags for the chrome command.')
  parser.add_option('--enable_ppapi_dev', dest='enable_ppapi_dev',
                    action='store', type='int', default=1,
                    help='Enable/disable PPAPI Dev interfaces while testing.')
  parser.add_option('--nacl_exe_stdin', dest='nacl_exe_stdin',
                    type='string', default=None,
                    help='Redirect standard input of NaCl executable.')
  parser.add_option('--nacl_exe_stdout', dest='nacl_exe_stdout',
                    type='string', default=None,
                    help='Redirect standard output of NaCl executable.')
  parser.add_option('--nacl_exe_stderr', dest='nacl_exe_stderr',
                    type='string', default=None,
                    help='Redirect standard error of NaCl executable.')
  parser.add_option('--expect_browser_process_crash',
                    dest='expect_browser_process_crash',
                    action='store_true',
                    help='Do not signal a failure if the browser process '
                    'crashes')
  parser.add_option('--enable_crash_reporter', dest='enable_crash_reporter',
                    action='store_true', default=False,
                    help='Force crash reporting on.')
  parser.add_option('--enable_sockets', dest='enable_sockets',
                    action='store_true', default=False,
                    help='Pass --allow-nacl-socket-api=<host> to Chrome, where '
                    '<host> is the name of the browser tester\'s web server.')

  return parser


def ProcessToolLogs(options, logs_dir):
  if options.tool == 'memcheck':
    analyzer = memcheck_analyze.MemcheckAnalyzer('', use_gdb=True)
    logs_wildcard = 'xml.*'
  files = glob.glob(os.path.join(logs_dir, logs_wildcard))
  retcode = analyzer.Report(files, options.url)
  return retcode


# An exception that indicates possible flake.
class RetryTest(Exception):
  pass


def DumpNetLog(netlog):
  sys.stdout.write('\n')
  if not os.path.isfile(netlog):
    sys.stdout.write('Cannot find netlog, did Chrome actually launch?\n')
  else:
    sys.stdout.write('Netlog exists (%d bytes).\n' % os.path.getsize(netlog))
    sys.stdout.write('Dumping it to stdout.\n\n\n')
    sys.stdout.write(open(netlog).read())
    sys.stdout.write('\n\n\n')


# Try to discover the real IP address of this machine.  If we can't figure it
# out, fall back to localhost.
# A windows bug makes using the loopback interface flaky in rare cases.
# http://code.google.com/p/chromium/issues/detail?id=114369
def GetHostName():
  host = 'localhost'
  try:
    host = socket.gethostbyname(socket.gethostname())
  except Exception:
    pass
  if host == '0.0.0.0':
    host = 'localhost'
  return host


def RunTestsOnce(url, options):
  # Set the default here so we're assured hard_timeout will be defined.
  # Tests, such as run_inbrowser_trusted_crash_in_startup_test, may not use the
  # RunFromCommand line entry point - and otherwise get stuck in an infinite
  # loop when something goes wrong and the hard timeout is not set.
  # http://code.google.com/p/chromium/issues/detail?id=105406
  if options.hard_timeout is None:
    options.hard_timeout = options.timeout * 4

  options.files.append(os.path.join(script_dir, 'browserdata', 'nacltest.js'))

  # Create server
  host = GetHostName()
  try:
    server = browsertester.server.Create(host, options.port)
  except Exception:
    sys.stdout.write('Could not bind %r, falling back to localhost.\n' % host)
    server = browsertester.server.Create('localhost', options.port)

  # If port 0 has been requested, an arbitrary port will be bound so we need to
  # query it.  Older version of Python do not set server_address correctly when
  # The requested port is 0 so we need to break encapsulation and query the
  # socket directly.
  host, port = server.socket.getsockname()

  file_mapping = dict(options.map_files)
  for filename in options.files:
    file_mapping[os.path.basename(filename)] = filename
  for _, real_path in file_mapping.items():
    if not os.path.exists(real_path):
      raise AssertionError('\'%s\' does not exist.' % real_path)
  mime_types = {}
  for ext, mime_type in options.mime_types:
    mime_types['.' + ext] = mime_type

  def ShutdownCallback():
    server.TestingEnded()
    close_browser = options.tool is not None and not options.interactive
    return close_browser

  listener = browsertester.rpclistener.RPCListener(ShutdownCallback)
  server.Configure(file_mapping,
                   dict(options.map_redirects),
                   mime_types,
                   options.allow_404,
                   options.bandwidth,
                   listener,
                   options.serving_dirs,
                   options.output_dir)

  browser = browsertester.browserlauncher.ChromeLauncher(options)

  full_url = 'http://%s:%d/%s' % (host, port, url)
  if len(options.test_args) > 0:
    full_url += '?' + urllib.urlencode(options.test_args)
  browser.Run(full_url, host, port)
  server.TestingBegun(0.125)

  # In Python 2.5, server.handle_request may block indefinitely.  Serving pages
  # is done in its own thread so the main thread can time out as needed.
  def Serve():
    while server.test_in_progress or options.interactive:
      server.handle_request()
  thread.start_new_thread(Serve, ())

  tool_failed = False
  time_started = time.time()

  def HardTimeout(total_time):
    return total_time >= 0.0 and time.time() - time_started >= total_time

  try:
    while server.test_in_progress or options.interactive:
      if not browser.IsRunning():
        if options.expect_browser_process_crash:
          break
        listener.ServerError('Browser process ended during test '
                             '(return code %r)' % browser.GetReturnCode())
        # If Chrome exits prematurely without making a single request to the
        # web server, this is probally a Chrome crash-on-launch bug not related
        # to the test at hand.  Retry, unless we're in interactive mode.  In
        # interactive mode the user may manually close the browser, so don't
        # retry (it would just be annoying.)
        if not server.received_request and not options.interactive:
          raise RetryTest('Chrome failed to launch.')
        else:
          break
      elif not options.interactive and server.TimedOut(options.timeout):
        js_time = server.TimeSinceJSHeartbeat()
        err = 'Did not hear from the test for %.1f seconds.' % options.timeout
        err += '\nHeard from Javascript %.1f seconds ago.' % js_time
        if js_time > 2.0:
          err += '\nThe renderer probably hung or crashed.'
        else:
          err += '\nThe test probably did not get a callback that it expected.'
        listener.ServerError(err)
        if not server.received_request:
          raise RetryTest('Chrome hung before running the test.')
        break
      elif not options.interactive and HardTimeout(options.hard_timeout):
        listener.ServerError('The test took over %.1f seconds.  This is '
                             'probably a runaway test.' % options.hard_timeout)
        break
      else:
        # If Python 2.5 support is dropped, stick server.handle_request() here.
        time.sleep(0.125)

    if options.tool:
      sys.stdout.write('##################### Waiting for the tool to exit\n')
      browser.WaitForProcessDeath()
      sys.stdout.write('##################### Processing tool logs\n')
      tool_failed = ProcessToolLogs(options, browser.tool_log_dir)

  finally:
    try:
      if listener.ever_failed and not options.interactive:
        if not server.received_request:
          sys.stdout.write('\nNo URLs were served by the test runner. It is '
                           'unlikely this test failure has anything to do with '
                           'this particular test.\n')
          DumpNetLog(browser.NetLogName())
    except Exception:
      listener.ever_failed = 1
    # Try to let the browser clean itself up normally before killing it.
    sys.stdout.write('##################### Terminating the browser\n')
    browser.WaitForProcessDeath()
    if browser.IsRunning():
      sys.stdout.write('##################### TERM failed, KILLING\n')
    # Always call Cleanup; it kills the process, but also removes the
    # user-data-dir.
    browser.Cleanup()
    # We avoid calling server.server_close() here because it causes
    # the HTTP server thread to exit uncleanly with an EBADF error,
    # which adds noise to the logs (though it does not cause the test
    # to fail).  server_close() does not attempt to tell the server
    # loop to shut down before closing the socket FD it is
    # select()ing.  Since we are about to exit, we don't really need
    # to close the socket FD.

  if tool_failed:
    return 2
  elif listener.ever_failed:
    return 1
  else:
    return 0


# This is an entrypoint for tests that treat the browser tester as a Python
# library rather than an opaque script.
# (e.g. run_inbrowser_trusted_crash_in_startup_test)
def Run(url, options):
  result = 1
  attempt = 1
  while True:
    try:
      result = RunTestsOnce(url, options)
      if result:
        # Currently (2013/11/15) nacl_integration is fairly flaky and there is
        # not enough time to look into it.  Retry if the test fails for any
        # reason.  Note that in general this test runner tries to only retry
        # when a known flake is encountered.  (See the other raise
        # RetryTest(..)s in this file.)  This blanket retry means that those
        # other cases could be removed without changing the behavior of the test
        # runner, but it is hoped that this blanket retry will eventually be
        # unnecessary and subsequently removed.  The more precise retries have
        # been left in place to preserve the knowledge.
        raise RetryTest('HACK retrying failed test.')
      break
    except RetryTest:
      # Only retry once.
      if attempt < 2:
        sys.stdout.write('\n@@@STEP_WARNINGS@@@\n')
        sys.stdout.write('WARNING: suspected flake, retrying test!\n\n')
        attempt += 1
        continue
      else:
        sys.stdout.write('\nWARNING: failed too many times, not retrying.\n\n')
        result = 1
        break
  return result


def RunFromCommandLine():
  parser = BuildArgParser()
  options, args = parser.parse_args()

  if len(args) != 0:
    print(args)
    parser.error('Invalid arguments')

  # Validate the URL
  url = options.url
  if url is None:
    parser.error('Must specify a URL')

  return Run(url, options)


if __name__ == '__main__':
  sys.exit(RunFromCommandLine())
