#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Interactive test script for the Chromoting host native messaging component.

import json
import readline
import struct
import subprocess
import sys


def PrintMenuAndGetBuilder(messages):
  print
  for i in range(0, len(messages)):
    print '%d: %s' % (i + 1, messages[i][0])
  print 'Q: Quit'
  while True:
    choice = raw_input('Enter choice: ')
    if choice.lower() == 'q':
      return None
    try:
      choice = int(choice)
    except ValueError:
      continue
    if choice >= 1 and choice <= len(messages):
      return messages[choice - 1][1]


def ReadMessage(in_stream):
  reply_length_bytes = in_stream.read(4)
  if len(reply_length_bytes) < 4:
    print 'Invalid message length'
    return None
  reply_length = struct.unpack('i', reply_length_bytes)[0]
  reply = in_stream.read(reply_length).decode('utf-8')
  if len(reply) != reply_length:
    print 'Invalid reply length'
    return None
  return json.loads(reply)


# Message builder methods.
def BuildHello():
  return {'type': 'hello'}


def BuildClearAllPairedClients():
  return {'type': 'clearPairedClients'}


def BuildDeletePairedClient():
  client_id = raw_input('Enter client id: ')
  return {'type': 'deletePairedClient',
          'clientId': client_id}


def BuildGetHostName():
  return {'type': 'getHostName'}


def BuildGetPinHash():
  host_id = raw_input('Enter host id: ')
  pin = raw_input('Enter PIN: ')
  return {'type': 'getPinHash',
          'hostId': host_id,
          'pin': pin}


def BuildGenerateKeyPair():
  return {'type': 'generateKeyPair'}


def BuildUpdateDaemonConfig():
  config_json = raw_input('Enter config JSON: ')
  return {'type': 'updateDaemonConfig',
          'config': config_json}


def BuildGetDaemonConfig():
  return {'type': 'getDaemonConfig'}


def BuildGetPairedClients():
  return {'type': 'getPairedClients'}


def BuildGetUsageStatsConsent():
  return {'type': 'getUsageStatsConsent'}


def BuildStartDaemon():
  while True:
    consent = raw_input('Report usage stats [y/n]? ')
    if consent.lower() == 'y':
      consent = True
    elif consent.lower() == 'n':
      consent = False
    else:
      continue
    break
  config_json = raw_input('Enter config JSON: ')
  return {'type': 'startDaemon',
          'consent': consent,
          'config': config_json}


def BuildStopDaemon():
  return {'type': 'stopDaemon'}


def BuildGetDaemonState():
  return {'type': 'getDaemonState'}


def BuildIt2MePermissionCheck():
  return {'type': 'it2mePermissionCheck'}


def main():
  if len(sys.argv) != 2:
    print 'Usage: ' + sys.argv[0] + ' <path to native messaging host>'
    sys.exit(1)

  native_messaging_host = sys.argv[1]

  child = subprocess.Popen(native_messaging_host, stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, close_fds=True)

  message_id = 0
  while True:
    messages = [
      ('Hello', BuildHello),
      ('Clear all paired clients', BuildClearAllPairedClients),
      ('Delete paired client', BuildDeletePairedClient),
      ('Get host name', BuildGetHostName),
      ('Get PIN hash', BuildGetPinHash),
      ('Generate key pair', BuildGenerateKeyPair),
      ('Update daemon config', BuildUpdateDaemonConfig),
      ('Get daemon config', BuildGetDaemonConfig),
      ('Get paired clients', BuildGetPairedClients),
      ('Get usage stats consent', BuildGetUsageStatsConsent),
      ('Start daemon', BuildStartDaemon),
      ('Stop daemon', BuildStopDaemon),
      ('Get daemon state', BuildGetDaemonState),
      ('It2Me permission check', BuildIt2MePermissionCheck),
    ]
    builder = PrintMenuAndGetBuilder(messages)
    if not builder:
      break
    message_dict = builder()
    message_dict['id'] = message_id
    message = json.dumps(message_dict)
    message_id += 1
    print 'Message: ' + message
    child.stdin.write(struct.pack('I', len(message)))
    child.stdin.write(message)
    child.stdin.flush()

    while True:
      reply_dict = ReadMessage(child.stdout)
      if reply_dict is None:
        break
      reply_pretty = json.dumps(reply_dict)
      if reply_dict['type'] == '_debug_log':
        print 'Log: ' + reply_pretty
        continue
      print 'Reply: ' + reply_pretty
      break

if __name__ == '__main__':
  main()
