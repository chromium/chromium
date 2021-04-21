#!/usr/bin/python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import requests
import subprocess
import time

def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir,
                                      os.pardir, os.pardir, os.pardir,
                                      os.pardir))

def GetDefaultBuildDir():
  return os.path.join(GetChromiumSrcDir(), 'out', 'Release-iphonesimulator')

def EnsureServerStarted(port, build_dir):
  # Check if the server is already running. If not, launch the server and wait
  # for it to be ready.
  server_url = 'http://localhost:' + port
  try:
    requests.post(server_url + '/session', json = {})
    response = requests.get(server_url + '/session/handles')
    assert response.status_code == 200
    # Use a page load timeout of 10 seconds.
    response = requests.post(
        server_url + '/session/timeouts', json = {'pageLoad': 10000})
    return response.status_code == 200
  except requests.exceptions.ConnectionError:
    cwt_chromedriver_path = os.path.join(os.path.dirname(__file__),
                                         'run_cwt_chromedriver.py')
    subprocess.Popen(cwt_chromedriver_path + ' --asan-build --build-dir ' +
                     build_dir + ' --port ' + port + ' >/dev/null 2>&1 &',
                     shell=True)
    time.sleep(15)

  # Wait up to 150 seconds for the server to be ready.
  for attempt in range (0, 150):
    try:
      requests.post(server_url + '/session', json= {})
      response = requests.get(server_url + '/session/handles')
      return response.status_code == 200
    except:
      if attempt == 149:
        return False
      else:
        time.sleep(1)

  return False

parser=argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--port', default='9999',
    help='The port to listen on for WebDriver commands')
parser.add_argument('--build-dir', default=GetDefaultBuildDir(),
    help='Chrome build directory')
parser.add_argument('filename', help='Input test file')
args = parser.parse_args()

server_started = EnsureServerStarted(args.port, args.build_dir)
assert server_started

# Construct a file:/// URL for the input test file.
if args.filename[0] == '/':
  test_url = 'file://' + args.filename
else:
  test_url = 'file://' + os.getcwd() + '/' + args.filename

# Run the test and extract its output.
request_body = {"url": test_url, "chrome_crashWaitTime": 2 }
request_url = 'http://localhost:' + args.port + '/session/chrome_crashtest'
response = requests.post(request_url, json = request_body)
assert response.status_code == 200
test_output = response.json()['value']['chrome_stderr']
print(test_output)
