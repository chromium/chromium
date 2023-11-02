#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Simple client/server script for generating an unlimited TCP stream.
# see shadow.sh for how it's intended to be used.

import socket
import sys
import thread
import time

sent = 0
received = 0

def Sink(socket):
  global received
  while True:
    tmp = socket.recv(4096)
    received += len(tmp)
    if not tmp:
      break;

def Spew(socket):
  global sent
  data = " " * 4096
  while True:
    tmp = socket.send(data)
    if tmp <= 0:
      break
    sent += tmp;

def PrintStats():
  global sent
  global received
  last_report = time.time()
  last_sent = 0
  last_received = 0
  while True:
    time.sleep(5)
    now = time.time();
    sent_now = sent
    received_now = received
    delta = now - last_report
    sent_mbps = ((sent_now - last_sent) * 8.0 / 1000000) / delta
    received_mbps = ((received_now - last_received) * 8.0 / 1000000) / delta
    print "Sent: %5.2f mbps  Received: %5.2f mbps" % (sent_mbps, received_mbps)
    last_report = now
    last_sent = sent_now
    last_received = received_now

def Serve(socket, upload=True, download=True):
  while True:
    (s, addr) = socket.accept()
    if upload:
      thread.start_new_thread(Spew, (s,))
    if download:
      thread.start_new_thread(Sink, (s,))

def Receiver(port, upload=True, download=True):
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  s.bind(('', port))
  s.listen(5)
  thread.start_new_thread(Serve, (s, upload, download))


def Connect(to_hostport, upload=True, download=False):
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  s.connect(to_hostport)
  if upload:
    thread.start_new_thread(Spew, (s,))
  if download:
    thread.start_new_thread(Sink, (s,))


def Usage():
  print "One of:"
  print "%s listen <port>" % sys.arv[0]
  print "%s upload <host> <port>" % sys.arv[0]
  print "%s download <host> <port>" % sys.arv[0]
  print "%s updown <host> <port>" % sys.arv[0]
  sys.exit(1)

if len(sys.argv) < 2:
  Usage()
if sys.argv[1] == "listen":
  Receiver(int(sys.argv[2]))
elif sys.argv[1] == "download":
  Connect( (sys.argv[2], int(sys.argv[3])), upload=False, download=True)
elif sys.argv[1] == "upload":
  Connect( (sys.argv[2], int(sys.argv[3])), upload=True, download=False)
elif sys.argv[1] == "updown":
  Connect( (sys.argv[2], int(sys.argv[3])), upload=True, download=True)
else:
  Usage()

PrintStats()
