# Copyright 2018 The Chromium Authors
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
from typing import Any, Dict, List, Optional, Tuple, Union

from gpu_tests.util import host_information

SummaryType = Dict[str, Dict[str, float]]
ResultType = Dict[str, Any]
MetricType = Dict[str, Union[List[str], List[float]]]


def LocateIPG() -> str:
  if host_information.IsWindows():
    ipg_dir = os.getenv('IPG_Dir')
    if not ipg_dir:
      raise Exception('No env IPG_Dir')
    gadget_path = os.path.join(ipg_dir, 'PowerLog3.0.exe')
    if not os.path.isfile(gadget_path):
      raise Exception("Can't locate Intel Power Gadget at " + gadget_path)
    return gadget_path
  if host_information.IsMac():
    return '/Applications/Intel Power Gadget/PowerLog'
  raise Exception('Only supported on Windows/Mac')


def GenerateIPGLogFilename(log_prefix: str = 'PowerLog',
                           log_dir: Optional[str] = None,
                           current_run: int = 1,
                           total_runs: int = 1,
                           timestamp: bool = False) -> str:
  # If all args take default value, it is the IPG's default log path.
  log_dir = log_dir or os.getcwd()
  log_dir = os.path.abspath(log_dir)
  if total_runs > 1:
    log_prefix = '%s_%d_%d' % (log_prefix, current_run, total_runs)
  if timestamp:
    now = datetime.datetime.now()
    log_prefix = '%s_%s' % (log_prefix, now.strftime('%Y%m%d%H%M%S'))
  return os.path.join(log_dir, log_prefix + '.csv')


def RunIPG(duration_in_s: int = 60,
           resolution_in_ms: int = 100,
           logfile: Optional[str] = None) -> None:
  intel_power_gadget_path = LocateIPG()
  command = ('"%s" -duration %d -resolution %d' %
             (intel_power_gadget_path, duration_in_s, resolution_in_ms))
  if not logfile:
    # It is not necessary but allows to print out the log path for debugging.
    logfile = GenerateIPGLogFilename()
  command = command + (' -file %s' % logfile)
  logging.debug('Running: %s', command)
  try:
    output = subprocess.check_output(command,
                                     shell=True,
                                     stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Running Intel Power Gadget failed. Output: %s', e.output)
    raise
  logging.debug('Running: DONE')
  logging.debug(output)


def AnalyzeIPGLogFile(logfile: Optional[str] = None,
                      skip_in_sec: int = 0) -> ResultType:
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
    for ii, index in enumerate(indices):
      sums[ii] += float(tokens[index])
  results = {'samples': samples}
  if samples > 0:
    for ii in range(0, len(indices)):
      results[labels[ii]] = sums[ii] / samples
  return results


def ProcessResultsFromMultipleIPGRuns(logfiles: List[str],
                                      skip_in_seconds: int = 0,
                                      outliers: int = 0,
                                      output_json: Optional[str] = None
                                      ) -> SummaryType:
  def _ScrapeDataFromIPGLogFiles() -> Tuple[Dict[str, ResultType], MetricType]:
    """Scrapes data from IPG log files.

    Returns:
      A tuple (per_core_results, metrics). |output| is a dictionary containing
      per-core results extracted from the IPG log files. |metrics| is a
      dictionary mapping metrics found in the logs to all found data points.
    """
    per_core_results = {}
    metrics = {}
    for logfile in logfiles:
      results = AnalyzeIPGLogFile(logfile, skip_in_seconds)
      results['log'] = logfile
      (_, filename) = os.path.split(logfile)
      (core, _) = os.path.splitext(filename)
      prefix = 'PowerLog_'
      if core.startswith(prefix):
        core = core[len(prefix):]
      per_core_results[core] = results

      for key in results:
        if key in ('samples', 'log'):
          continue
        metrics.setdefault(key, []).append(results[key])
    return per_core_results, metrics

  def _CalculateSummaryStatistics(metrics: MetricType) -> SummaryType:
    """Calculates summary statistics for the given metrics.

    Args:
      metrics: A dictionary mapping metrics to lists of data points.

    Returns:
      A dictionary mapping the same metrics in |metrics| to dicts containing
      the 'mean' and 'stdev' for the metric.
    """
    summary = {}
    for key, data in metrics.items():
      assert data and len(data) > 1
      n = len(data)
      if outliers > 0:
        assert outliers * 2 < n
        data.sort()
        data = data[outliers:(n - outliers)]
        n = len(data)
      logging.debug('%s: valid samples = %d', key, n)
      mean = sum(data) / float(n)
      ss = sum((x - mean)**2 for x in data)
      stdev = (ss / float(n))**0.5
      summary[key] = {
          'mean': mean,
          'stdev': stdev,
      }
    return summary

  assert len(logfiles) > 1
  output, metrics = _ScrapeDataFromIPGLogFiles()
  summary = _CalculateSummaryStatistics(metrics)
  output['summary'] = summary

  if output_json:
    json_file = open(output_json, 'w')
    json_file.write(json.dumps(output, indent=4))
    json_file.close()

  return summary
