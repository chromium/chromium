# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script implements a few IntelPowerGadget related helper functions.

This script only works on Windows/Mac with Intel CPU. Intel Power Gadget needs
to be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget

An easy way to use the APIs are:
1) Launch your program.
2) Call RunIPG() with no args. It will automatically locate the IPG installed
   on the machine.
3) Call AnalyzeIPGLogFile() with no args. It will analyze the default IPG log
   file, which is PowerLog.csv at current dir; then it will print out the power
   usage summary. If you want to skip a few seconds of the power log data, say,
   5 seconds, call AnalyzeIPGLogFile(skip_in_sec=5).
"""

import datetime
import json
import logging
import os
import subprocess
import sys

def LocateIPG():
  if sys.platform == 'win32':
    ipg_dir = os.getenv('IPG_Dir')
    if not ipg_dir:
      raise Exception("No env IPG_Dir")
    gadget_path = os.path.join(ipg_dir, "PowerLog3.0.exe")
    if not os.path.isfile(gadget_path):
      raise Exception("Can't locate Intel Power Gadget at " + gadget_path)
    return gadget_path
  if sys.platform == 'darwin':
    return '/Applications/Intel Power Gadget/PowerLog'
  raise Exception("Only supported on Windows/Mac")


def GenerateIPGLogFilename(log_prefix='PowerLog', log_dir=None, current_run=1,
                           total_runs=1, timestamp=False):
  # If all args take default value, it is the IPG's default log path.
  log_dir = log_dir or os.getcwd()
  log_dir = os.path.abspath(log_dir)
  if total_runs > 1:
    log_prefix = "%s_%d_%d" % (log_prefix, current_run, total_runs)
  if timestamp:
    now = datetime.datetime.now()
    log_prefix = "%s_%s" % (log_prefix, now.strftime('%Y%m%d%H%M%S'))
  return os.path.join(log_dir, log_prefix + '.csv')


def RunIPG(duration_in_s=60, resolution_in_ms=100, logfile=None):
  intel_power_gadget_path = LocateIPG()
  command = ('"%s" -duration %d -resolution %d' %
             (intel_power_gadget_path, duration_in_s, resolution_in_ms))
  if not logfile:
    # It is not necessary but allows to print out the log path for debugging.
    logfile = GenerateIPGLogFilename()
  command = command + (' -file %s' %logfile)
  logging.debug("Running: " + command)
  output = subprocess.check_output(command, shell=True)
  logging.debug("Running: DONE")
  logging.debug(output)


def AnalyzeIPGLogFile(logfile=None, skip_in_sec=0):
  if not logfile:
    logfile = GenerateIPGLogFilename()
  if not os.path.isfile(logfile):
    raise Exception("Can't locate logfile at " + logfile)
  first_line = True
  samples = 0
  cols = 0
  indices = []
  labels = []
  sums = []
  col_time = None
  for line in open(logfile):
    tokens = [token.strip('" ') for token in line.split(',')]
    if first_line:
      first_line = False
      cols = len(tokens)
      for ii in range(0, cols):
        token = tokens[ii]
        if token.startswith('Elapsed Time'):
          col_time = ii
        elif token.endswith('(Watt)'):
          indices.append(ii)
          labels.append(token[:-len('(Watt)')])
          sums.append(0.0)
      assert col_time
      assert cols > 0
      assert len(indices) > 0
      continue
    if len(tokens) != cols:
      continue
    if skip_in_sec > 0 and float(tokens[col_time]) < skip_in_sec:
      continue
    samples += 1
    for ii in range(0, len(indices)):
      index = indices[ii]
      sums[ii] += float(tokens[index])
  results = {'samples': samples}
  if samples > 0:
    for ii in range(0, len(indices)):
      results[labels[ii]] = sums[ii] / samples
  return results


def ProcessResultsFromMultipleIPGRuns(logfiles, skip_in_seconds=0,
                                      outliers=0, output_json=None):
  assert len(logfiles) > 1
  output = {}
  summary = {}
  for logfile in logfiles:
    results = AnalyzeIPGLogFile(logfile, skip_in_seconds)
    results['log'] = logfile
    (_, filename) = os.path.split(logfile)
    (core, _) = os.path.splitext(filename)
    prefix = 'PowerLog_'
    if core.startswith(prefix):
      core = core[len(prefix):]
    output[core] = results

    for key in results:
      if key == 'samples' or key == 'log':
        continue
      if not key in summary:
        summary[key] = [results[key]]
      else:
        summary[key].append(results[key])

  for key in summary:
    data = summary[key]
    assert data and len(data) > 1
    n = len(data)
    if outliers > 0:
      assert outliers * 2 < n
      data.sort()
      data = data[outliers:(n - outliers)]
      n = len(data)
    logging.debug('%s: valid samples = %d', key, n)
    mean = sum(data) / float(n)
    ss = sum((x - mean) ** 2 for x in data)
    stdev = (ss / float(n)) ** 0.5
    summary[key] = {
      'mean': mean,
      'stdev': stdev,
    }
  output['summary'] = summary

  if output_json:
    json_file = open(output_json, 'w')
    json_file.write(json.dumps(output, indent=4))
    json_file.close()

  return summary
