# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import glob
import hashlib
import itertools
import io
import json
import logging
import multiprocessing
import os
import shutil
import subprocess
import tempfile
from typing import Dict, List, Optional, Set, Tuple

from PIL import Image  # pylint: disable=import-error

import requests  # pylint: disable=import-error

from gold_inexact_matching import common_typing as ct
from gold_inexact_matching import parameter_set

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
GOLDCTL_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'linux', 'goldctl'),
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'mac', 'goldctl'),
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'win',
                 'goldctl.exe'),
]

# The downloaded expectations are in the following format:
# {
#   branch (str): {
#     test_name (str): {
#       digest1 (str): status (str),
#       digest2 (str): status (str),
#       ...
#     }
#   }
# }
ExpectationJson = Dict[str, Dict[str, Dict[str, str]]]


class BaseParameterOptimizer():
  """Abstract base class for running a parameter optimization for a test."""
  MIN_EDGE_THRESHOLD = 0
  MAX_EDGE_THRESHOLD = 255
  MIN_MAX_DIFF = 0
  MIN_DELTA_THRESHOLD = 0
  # 4 channels, ranging from 0-255 each.
  MAX_DELTA_THRESHOLD = 255 * 4

  def __init__(self, args: ct.ParsedCmdArgs, test_name: str):
    """
    Args:
      args: The parse arguments from an argparse.ArgumentParser.
      test_name: The name of the test to optimize.
    """
    self._args = args
    self._test_name = test_name
    self._goldctl_binary: Optional[str] = None
    self._working_dir: Optional[str] = None
    self._expectations: Optional[ExpectationJson] = None
    # TODO(skbug.com/10610): Switch away from the public instance once
    # authentication is fixed for the non-public instance.
    self._gold_url = 'https://%s-public-gold.skia.org' % args.gold_instance
    self._pool = multiprocessing.Pool()
    # A map of strings, denoting a resolution or trace, to a set of strings,
    # denoting images that are that dimension or belong to that trace.
    self._images: Dict[str, Set[str]] = collections.defaultdict(set)
    self._VerifyArgs()
    parameter_set.ParameterSet.ignored_border_thickness = \
        self._args.ignored_border_thickness

  @classmethod
  def AddArguments(cls, parser: ct.CmdArgParser) -> ct.ArgumentGroupTuple:
    """Add optimizer-specific arguments to the parser.

    Args:
      parser: An argparse.ArgumentParser instance.

    Returns:
      A 3-tuple (common_group, sobel_group, fuzzy_group). All three are
      argument groups of |parser| corresponding to arguments for any sort of
      inexact matching algorithm, arguments specific to Sobel filter matching,
      and arguments specific to fuzzy matching.
    """
    common_group = parser.add_argument_group('Common Arguments')
    common_group.add_argument(
        '--test',
        required=True,
        action='append',
        dest='test_names',
        help='The name of a test to find parameter values for, as reported in '
        'the Skia Gold UI. Can be passed multiple times to run optimizations '
        'for multiple tests.')
    common_group.add_argument('--gold-instance',
                              default='chrome',
                              help='The Skia Gold instance to interact with.')
    common_group.add_argument(
        '--corpus',
        default='chrome-gpu',
        help='The corpus within the instance to interact with.')
    common_group.add_argument(
        '--target-success-percent',
        default=100,
        type=float,
        help='The percentage of comparisons that need to succeed in order for '
        'a set of parameters to be considered good.')
    common_group.add_argument(
        '--no-cleanup',
        action='store_true',
        default=False,
        help="Don't clean up the temporary files left behind by the "
        'optimization process.')
    common_group.add_argument(
        '--group-images-by-resolution',
        action='store_true',
        default=False,
        help='Group images for comparison based on resolution instead of by '
        'Gold trace. This will likely add some noise, as some comparisons will '
        'be made that Gold would not consider, but this has the benefit of '
        'optimizing over all historical data instead of only over data in '
        'the past several hundred commits. Note that this will likely '
        'result in a significantly longer runtime.')

    sobel_group = parser.add_argument_group(
        'Sobel Arguments',
        'To disable Sobel functionality, set both min and max edge thresholds '
        'to 255.')
    sobel_group.add_argument(
        '--min-edge-threshold',
        default=10,
        type=int,
        help='The minimum value to consider for the Sobel edge threshold. '
        'Lower values result in more of the image being blacked out before '
        'comparison.')
    sobel_group.add_argument(
        '--max-edge-threshold',
        default=255,
        type=int,
        help='The maximum value to consider for the Sobel edge threshold. '
        'Higher values result in less of the image being blacked out before '
        'comparison.')

    fuzzy_group = parser.add_argument_group(
        'Fuzzy Arguments',
        'To disable Fuzzy functionality, set min/max for both parameters to 0')
    fuzzy_group.add_argument(
        '--min-max-different-pixels',
        dest='min_max_diff',
        default=0,
        type=int,
        help='The minimum value to consider for the maximum number of '
        'different pixels. Lower values result in less fuzzy comparisons being '
        'allowed.')
    fuzzy_group.add_argument(
        '--max-max-different-pixels',
        dest='max_max_diff',
        default=50,
        type=int,
        help='The maximum value to consider for the maximum number of '
        'different pixels. Higher values result in more fuzzy comparisons '
        'being allowed.')
    fuzzy_group.add_argument(
        '--min-delta-threshold',
        default=0,
        type=int,
        help='The minimum value to consider for the per-channel delta sum '
        'threshold. Lower values result in less fuzzy comparisons being '
        'allowed.')
    fuzzy_group.add_argument(
        '--max-delta-threshold',
        default=30,
        type=int,
        help='The maximum value to consider for the per-channel delta sum '
        'threshold. Higher values result in more fuzzy comparisons being '
        'allowed.')
    fuzzy_group.add_argument(
        '--ignored-border-thickness',
        default=0,
        type=int,
        help='How many pixels along the border of the image to ignore. 0 is '
        'typical for most tests, 1 is useful for tests that have edges going '
        'all the way to the border of the image and are using a Sobel filter.')

    return common_group, sobel_group, fuzzy_group

  def _VerifyArgs(self) -> None:
    """Verifies that the provided arguments are valid for an optimizer."""
    assert self._args.target_success_percent > 0
    assert self._args.target_success_percent <= 100

    assert self._args.min_edge_threshold >= self.MIN_EDGE_THRESHOLD
    assert self._args.max_edge_threshold <= self.MAX_EDGE_THRESHOLD
    assert self._args.min_edge_threshold <= self._args.max_edge_threshold

    assert self._args.min_max_diff >= self.MIN_MAX_DIFF
    assert self._args.min_max_diff <= self._args.max_max_diff
    assert self._args.min_delta_threshold >= self.MIN_DELTA_THRESHOLD
    assert self._args.max_delta_threshold <= self.MAX_DELTA_THRESHOLD
    assert self._args.min_delta_threshold <= self._args.max_delta_threshold
    assert self._args.ignored_border_thickness >= 0

  def RunOptimization(self) -> None:
    """Runs an optimization for whatever test and parameters were supplied.

    The results should be printed to stdout when they are available.
    """
    self._working_dir = tempfile.mkdtemp()
    try:
      self._DownloadData()

      # Do a preliminary test to make sure that the most permissive
      # parameters can succeed.
      logging.info('Verifying initial parameters')
      success, num_pixels, max_delta = self._RunComparisonForParameters(
          self._GetMostPermissiveParameters())
      if not success:
        raise RuntimeError(
            'Most permissive parameters did not result in a comparison '
            'success. Try loosening parameters or lowering target success '
            'percent. Max differing pixels: %d, max delta: %s' % (num_pixels,
                                                                  max_delta))

      self._RunOptimizationImpl()

    finally:
      if not self._args.no_cleanup:
        shutil.rmtree(self._working_dir)
        # Cleanup files left behind by "goldctl match"
        for f in glob.iglob(os.path.join(tempfile.gettempdir(), 'goldctl-*')):
          shutil.rmtree(f)

  def _RunOptimizationImpl(self) -> None:
    """Runs the algorithm-specific optimization code for an optimizer."""
    raise NotImplementedError()

  def _GetMostPermissiveParameters(self) -> parameter_set.ParameterSet:
    return parameter_set.ParameterSet(self._args.max_max_diff,
                                      self._args.max_delta_threshold,
                                      self._args.min_edge_threshold)

  def _DownloadData(self) -> None:
    """Downloads all the necessary data for a test."""
    assert self._working_dir
    logging.info('Downloading images')
    if self._args.group_images_by_resolution:
      self._DownloadExpectations('%s/json/v2/expectations' % self._gold_url)
      self._DownloadImagesForResolutionGrouping()
    else:
      # A grouping ID is an MD5 hash of a JSON object containing the corpus and
      # name of a test with its keys sorted alphabetically.
      grouping_dict = {
          'name': self._test_name,
          'source_type': self._args.corpus,
      }
      # Specify separators to avoid the automatic whitespace, which Go/Gold
      # does.
      json_str = json.dumps(grouping_dict,
                            sort_keys=True,
                            separators=(',', ':'))
      md5 = hashlib.md5()
      md5.update(json_str.encode('utf-8'))
      self._DownloadExpectations('%s/json/v1/positivedigestsbygrouping/%s' %
                                 (self._gold_url, md5.hexdigest()))
      self._DownloadImagesForTraceGrouping()
    for grouping, digests in self._images.items():
      logging.info('Found %d images for group %s', len(digests), grouping)
      logging.debug('Digests: %r', digests)

  def _DownloadExpectations(self, url: str) -> None:
    """Downloads the expectation JSON from Gold into memory."""
    logging.info('Downloading expectations JSON')
    r = requests.get(url)
    assert r.status_code == 200
    self._expectations = r.json()

  def _DownloadImagesForResolutionGrouping(self) -> None:
    """Downloads all the positive images for a test to disk.

    Images are grouped by resolution.
    """
    assert self._expectations
    test_expectations = self._expectations.get('primary',
                                               {}).get(self._test_name, {})
    positive_digests = [
        digest for digest, status in test_expectations.items()
        if status == 'positive'
    ]
    if not positive_digests:
      raise RuntimeError('Failed to find any positive digests for test %s' %
                         self._test_name)
    for digest in positive_digests:
      content = self._DownloadImageWithDigest(digest)
      image = Image.open(io.BytesIO(content))
      self._images['%dx%d' % (image.size[0], image.size[1])].add(digest)

  def _DownloadImagesForTraceGrouping(self) -> None:
    """Download all recent positive images for a test to disk.

    Images are grouped by Skia Gold trace ID, i.e. each hardware/software
    combination is a separate group.
    """
    assert self._expectations
    # The downloaded trace data contains a list of traces, each with a list of
    # digests. The digests should be unique within each trace, but convert to
    # sets just to be sure.
    for trace in self._expectations['traces']:
      trace_id = trace['trace_id']
      digests = set(trace['digests'])
      if not digests:
        logging.warning(
            'Failed to find any positive digests for test %s and trace %s. '
            'This is likely due to the trace being old.', self._test_name,
            trace_id)
      self._images[trace_id] = digests
      for d in digests:
        self._DownloadImageWithDigest(d)

  def _DownloadImageWithDigest(self, digest: str) -> bytes:
    """Downloads an image with the given digest and saves it to disk.

    Args:
      digest: The md5 digest of the image to download.

    Returns:
      A copy of the image content that was written to disk as bytes.
    """
    logging.debug('Downloading image %s.png', digest)
    r = requests.get('%s/img/images/%s.png' % (self._gold_url, digest))
    assert r.status_code == 200
    with open(self._GetImagePath(digest), 'wb') as outfile:
      outfile.write(r.content)
    return r.content

  def _GetImagePath(self, digest: str) -> str:
    """Gets a filepath to an image based on digest.

    Args:
      digest: The md5 digest of the image, as provided by Gold.

    Returns:
      A string containing a filepath to where the image should be on disk.
    """
    return os.path.join(self._working_dir, '%s.png' % digest)

  def _GetGoldctlBinary(self) -> str:
    """Gets the filepath to the goldctl binary to use.

    Returns:
      A string containing a filepath to the goldctl binary to use.
    """
    if not self._goldctl_binary:
      for path in GOLDCTL_PATHS:
        if os.path.isfile(path):
          self._goldctl_binary = path
          break
      if not self._goldctl_binary:
        raise RuntimeError(
            'Could not find goldctl binary. Checked %s' % GOLDCTL_PATHS)
    return self._goldctl_binary

  def _RunComparisonForParameters(
      self, parameters: parameter_set.ParameterSet) -> Tuple[bool, int, int]:
    """Runs a comparison for all image combinations using some parameters.

    Args:
      parameters: A parameter_set.ParameterSet instance containing parameters to
          use.

    Returns:
      A 3-tuple (success, num_pixels, max_diff). |success| is a boolean
      denoting whether enough comparisons succeeded to meet the desired success
      percentage. |num_pixels| is an int denoting the maximum number of pixels
      that did not match across all comparisons. |max_delta| is the maximum
      per-channel delta sum across all comparisons.
    """
    logging.debug('Running comparison for parameters: %s', parameters)
    num_attempts = 0
    num_successes = 0
    max_num_pixels = -1
    max_max_delta = -1

    for resolution, digest_list in self._images.items():
      logging.debug('Resolution/trace: %s, digests: %s', resolution,
                    digest_list)
      cmds = [
          self._GenerateComparisonCmd(l, r, parameters)
          for (l, r) in itertools.combinations(digest_list, 2)
      ]
      results = self._pool.map(RunCommandAndExtractData, cmds)
      for (success, num_pixels, max_delta) in results:
        num_attempts += 1
        if success:
          num_successes += 1
        max_num_pixels = max(num_pixels, max_num_pixels)
        max_max_delta = max(max_delta, max_max_delta)

    # This could potentially happen if run on a test where there's only one
    # positive image per resolution/trace.
    if num_attempts == 0:
      num_attempts = 1
      num_successes = 1
    success_percent = float(num_successes) * 100 / num_attempts
    logging.debug('Success percent: %s', success_percent)
    logging.debug('target success percent: %s',
                  self._args.target_success_percent)
    successful = success_percent >= self._args.target_success_percent
    logging.debug(
        'Successful: %s, Max different pixels: %d, Max per-channel delta sum: '
        '%d', successful, max_num_pixels, max_max_delta)
    return successful, max_num_pixels, max_max_delta

  def _GenerateComparisonCmd(
      self, left_digest: str, right_digest: str,
      parameters: parameter_set.ParameterSet) -> List[str]:
    """Generates a comparison command for the given arguments.

    The returned command can be passed directly to a subprocess call.

    Args:
      left_digest: The first/left image digest to compare.
      right_digest: The second/right image digest to compare.
      parameters: A parameter_set.ParameterSet instance containing the
          parameters to use for image comparison.

    Returns:
      A list of strings specifying a goldctl command to compare |left_digest|
      to |right_digest| using the parameters in |parameters|.
    """
    cmd = [
        self._GetGoldctlBinary(),
        'match',
        self._GetImagePath(left_digest),
        self._GetImagePath(right_digest),
        '--algorithm',
        'sobel',
    ] + parameters.AsList()
    return cmd


def RunCommandAndExtractData(cmd: List[str]) -> Tuple[bool, int, int]:
  """Runs a comparison command and extracts data from it.

  This is outside of the parameter optimizers because it is meant to be run via
  multiprocessing.Pool.map(), which does not play nice with class methods since
  they can't be easily pickled.

  Args:
    cmd: A list of strings containing the command to run.

  Returns:
    A 3-tuple (success, num_pixels, max_delta). |success| is a boolean denoting
    whether the comparison succeeded or not. |num_pixels| is an int denoting
    the number of pixels that did not match. |max_delta| is the maximum
    per-channel delta sum in the comparison.
  """
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  if not isinstance(output, str):
    output = output.decode('utf-8')
  success = False
  num_pixels = 0
  max_delta = 0
  for line in output.splitlines():
    if 'Images match.' in line:
      success = True
    if 'Number of different pixels' in line:
      num_pixels = int(line.split(':')[1])
    if 'Maximum per-channel delta sum' in line:
      max_delta = int(line.split(':')[1])
  logging.debug('Result for %r: success: %s, num_pixels: %d, max_delta: %d',
                cmd, success, num_pixels, max_delta)
  return success, num_pixels, max_delta
