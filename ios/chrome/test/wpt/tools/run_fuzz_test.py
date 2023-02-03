#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import psutil
import re
import requests
import subprocess
import time

def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir,
                                      os.pardir, os.pardir, os.pardir,
                                      os.pardir))

def GetDefaultBuildDir():
  return GetChromiumSrcDir()

def StartServer(*, port, build_dir, device, os_version):
  cwt_chromedriver_path = os.path.join(os.path.dirname(__file__),
                                       'run_cwt_chromedriver.py')
  subprocess.Popen(cwt_chromedriver_path + ' --asan-build --build-dir ' +
                   build_dir + ' --port ' + port + ' --device "' + device + '"'
                   ' --os ' + os_version + ' >/dev/null 2>&1 &',
                   shell=True)
  time.sleep(15)

  # Wait up to 150 seconds for the server to be ready.
  server_url = 'http://localhost:' + port
  for attempt in range (0, 150):
    try:
      requests.post(server_url + '/session', json= {})
      response = requests.get(server_url + '/session/handles')
      assert response.status_code == 200
      # Use a page load timeout of 10 seconds.
      response = requests.post(
        server_url + '/session/timeouts', json = {'pageLoad': 10000})
      return response.status_code == 200
    except:
      if attempt == 149:
        return False
      else:
        time.sleep(1)

def IsCurrentVersion(*, version, build_dir):
  plist_path = os.path.join(build_dir, 'ios_cwt_chromedriver_tests.app',
                            'Info.plist')
  version_command = 'defaults read ' + plist_path + ' CFBundleVersion'
  completed_process = subprocess.run(version_command, shell=True,
                                     capture_output=True)
  current_version = completed_process.stdout.decode('utf-8').strip()

  return version == current_version

def KillServer():
  # Gather all running run_cwt_chromedriver.py and xcodebuild instances. There
  # should only ever be at most one process of each type, but handle the case
  # of multiple such processes for extra robustness.
  cwt_chromedriver_procs = []
  xcodebuild_procs = []
  for proc in psutil.process_iter(attrs=['pid', 'cmdline', 'name'],
                                  ad_value=[]):
    cmd_line = proc.info['cmdline']
    if proc.info['name'] == 'xcodebuild':
      xcodebuild_procs.append(proc)
    elif len(cmd_line) > 1 and "run_cwt_chromedriver.py" in cmd_line[1]:
      cwt_chromedriver_procs.append(proc)

  # It's important to kill instances of run_cwt_chromedriver.py before killing
  # xcodebuild, since if xcodebuild is killed first, run_cwt_chromedriver.py
  # will detect this and launch another instance.
  for proc in cwt_chromedriver_procs:
    proc.kill()

  for proc in xcodebuild_procs:
    proc.kill()

def EnsureServerStarted(*, port, build_dir, device, os_version):
  # Check if the server is already running. If not, launch the server and wait
  # for it to be ready. If the server is running but its version doesn't match
  # the current build, kill the running server and relaunch.
  server_url = 'http://localhost:' + port
  try:
    requests.post(server_url + '/session', json = {})
    response = requests.get(server_url + '/session/chrome_versionInfo')
    assert response.status_code == 200
    chrome_version = response.json()['value']['browserVersion']
    if IsCurrentVersion(version=chrome_version, build_dir=build_dir):
      return True
    else:
      KillServer()
      return StartServer(port=port, build_dir=build_dir, device=device,
                         os_version=os_version)
  except requests.exceptions.ConnectionError:
    return StartServer(port=port, build_dir=build_dir, device=device,
                       os_version=os_version)

parser=argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--port', default='9999',
    help='The port to listen on for WebDriver commands')
parser.add_argument('--build-dir', default=GetDefaultBuildDir(),
    help='Chrome build directory')
parser.add_argument('--os', required=True, help='iOS version')
parser.add_argument('--device', required=True, help='Device type')
parser.add_argument('filename', help='Input test file')
args = parser.parse_args()

server_started = EnsureServerStarted(port=args.port, build_dir=args.build_dir,
                                     device=args.device, os_version=args.os)
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
