#!/usr/bin/env python

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import datetime
import json
import os
import shlex
import subprocess
import sys
from optparse import OptionParser

"""Start a client to fetch web pages either using wget or using quic_client.
If --use_wget is set, it uses wget.
Usage: This invocation
  run_client.py --quic_binary_dir=../../../../out/Debug \
      --address=127.0.0.1 --port=5000 --infile=test_urls.json \
      --delay_file=delay.csv --packets_file=packets.csv
  fetches pages listed in test_urls.json from a quic server running at
  127.0.0.1 on port 5000 using quic binary ../../../../out/Debug/quic_client
  and stores the delay in delay.csv and the max received packet number (for
  QUIC) in packets.csv.
  If --use_wget is present, it will fetch the URLs using wget and ignores
  the flags --address, --port, --quic_binary_dir, etc.
"""

def Timestamp(datetm=None):
  """Get the timestamp in microseconds.
  Args:
    datetm: the date and time to be converted to timestamp.
      If not set, use the current UTC time.
  Returns:
    The timestamp in microseconds.
  """
  datetm = datetm or datetime.datetime.utcnow()
  diff = datetm - datetime.datetime.utcfromtimestamp(0)
  timestamp = (diff.days * 86400 + diff.seconds) * 1000000 + diff.microseconds
  return timestamp

class PageloadExperiment:
  def __init__(self, use_wget, quic_binary_dir, quic_server_address,
               quic_server_port):
    """Initialize PageloadExperiment.

    Args:
      use_wget: Whether to use wget.
      quic_binary_dir: Directory for quic_binary.
      quic_server_address: IP address of quic server.
      quic_server_port: Port of the quic server.
    """
    self.use_wget = use_wget
    self.quic_binary_dir = quic_binary_dir
    self.quic_server_address = quic_server_address
    self.quic_server_port = quic_server_port
    if not use_wget and not os.path.isfile(quic_binary_dir + '/quic_client'):
      raise IOError('There is no quic_client in the given dir: %s.'
                    % quic_binary_dir)

  @classmethod
  def ReadPages(cls, json_file):
    """Return the list of URLs from the json_file.

    One entry of the list may contain a html link and multiple resources.
    """
    page_list = []
    with open(json_file) as f:
      data = json.load(f)
      for page in data['pages']:
        url = page['url']
        if 'resources' in page:
          resources = page['resources']
        else:
          resources = None
        if not resources:
          page_list.append([url])
        else:
          urls = [url]
          # For url http://x.com/z/y.html, url_dir is http://x.com/z
          url_dir = url.rsplit('/', 1)[0]
          for resource in resources:
            urls.append(url_dir + '/' + resource)
          page_list.append(urls)
    return page_list

  def DownloadOnePage(self, urls):
    """Download a page emulated by a list of urls.

    Args:
      urls: list of URLs to fetch.
    Returns:
      A tuple (page download time, max packet number).
    """
    if self.use_wget:
      cmd = 'wget -O -'
    else:
      cmd = '%s/quic_client --port=%s --address=%s' % (
          self.quic_binary_dir, self.quic_server_port, self.quic_server_address)
    cmd_in_list = shlex.split(cmd)
    cmd_in_list.extend(urls)
    start_time = Timestamp()
    ps_proc = subprocess.Popen(cmd_in_list,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    _std_out, std_err = ps_proc.communicate()
    end_time = Timestamp()
    delta_time = end_time - start_time
    max_packets = 0
    if not self.use_wget:
      for line in std_err.splitlines():
        if line.find('Client: Got packet') >= 0:
          elems = line.split()
          packet_num = int(elems[4])
          max_packets = max(max_packets, packet_num)
    return delta_time, max_packets

  def RunExperiment(self, infile, delay_file, packets_file=None, num_it=1):
    """Run the pageload experiment.

    Args:
      infile: Input json file describing the page list.
      delay_file: Output file storing delay in csv format.
      packets_file: Output file storing max packet number in csv format.
      num_it: Number of iterations to run in this experiment.
    """
    page_list = self.ReadPages(infile)
    header = [urls[0].rsplit('/', 1)[1] for urls in  page_list]
    header0 = 'wget' if self.use_wget else 'quic'
    header = [header0] + header

    plt_list = []
    packets_list = []
    for i in range(num_it):
      plt_one_row = [str(i)]
      packets_one_row = [str(i)]
      for urls in page_list:
        time_micros, num_packets = self.DownloadOnePage(urls)
        time_secs = time_micros / 1000000.0
        plt_one_row.append('%6.3f' % time_secs)
        packets_one_row.append('%5d' % num_packets)
      plt_list.append(plt_one_row)
      packets_list.append(packets_one_row)

    with open(delay_file, 'w') as f:
      csv_writer = csv.writer(f, delimiter=',')
      csv_writer.writerow(header)
      for one_row in plt_list:
        csv_writer.writerow(one_row)
    if packets_file:
      with open(packets_file, 'w') as f:
        csv_writer = csv.writer(f, delimiter=',')
        csv_writer.writerow(header)
        for one_row in packets_list:
          csv_writer.writerow(one_row)


def main():
  parser = OptionParser()
  parser.add_option('--use_wget', dest='use_wget', action='store_true',
                    default=False)
  # Note that only debug version generates the log containing packets
  # information.
  parser.add_option('--quic_binary_dir', dest='quic_binary_dir',
                    default='../../../../out/Debug')
  # For whatever server address you specify, you need to run the
  # quic_server on that machine and populate it with the cache containing
  # the URLs requested in the --infile.
  parser.add_option('--address', dest='quic_server_address',
                    default='127.0.0.1')
  parser.add_option('--port', dest='quic_server_port',
                    default='5002')
  parser.add_option('--delay_file', dest='delay_file', default='delay.csv')
  parser.add_option('--packets_file', dest='packets_file',
                    default='packets.csv')
  parser.add_option('--infile', dest='infile', default='test_urls.json')
  (options, _) = parser.parse_args()

  exp = PageloadExperiment(options.use_wget, options.quic_binary_dir,
                           options.quic_server_address,
                           options.quic_server_port)
  exp.RunExperiment(options.infile, options.delay_file, options.packets_file)

if __name__ == '__main__':
  sys.exit(main())
