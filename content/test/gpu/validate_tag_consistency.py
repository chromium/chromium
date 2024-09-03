#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to ensure that the same tags are in all expectation files."""

from __future__ import print_function

import argparse
import logging
import os
import textwrap
from typing import Dict, List
import sys

# Constants for tag set generation.
LINE_START = '# '
TAG_SET_START = 'tags: [ '
TAG_SET_END = ' ]'
DEFAULT_LINE_LENGTH = 80 - len(LINE_START) - len(TAG_SET_START)
BREAK_INDENTATION = ' ' * 4

# Certain tags are technically subsets of other tags, e.g. win10 falls under the
# general win umbrella. We take this into account when checking for conflicting
# expectations, so we store the source of truth here and generate the resulting
# strings for the tag header.
TAG_SPECIALIZATIONS = {
    'OS_TAGS': {
        'android': [
            'android-oreo',
            'android-pie',
            'android-r',
            'android-s',
            'android-t',
            'android-14',
        ],
        'chromeos': [],
        'fuchsia': [],
        'linux': [
            'ubuntu',
        ],
        'mac': [
            'highsierra',
            'mojave',
            'catalina',
            'bigsur',
            'monterey',
            'ventura',
            'sonoma',
            'sequoia',
        ],
        'win': [
            'win8',
            'win10',
            'win11',
        ],
    },
    'BROWSER_TAGS': {
        'android-chromium': [],
        'android-webview-instrumentation': [],
        'debug': [
            'debug-x64',
        ],
        'release': [
            'release-x64',
        ],
        # These two are both Fuchsia-related.
        'fuchsia-chrome': [],
        'web-engine-shell': [],
        # These two are both ChromeOS-related.
        'lacros-chrome': [],
        'cros-chrome': [],
    },
    'GPU_TAGS': {
        'amd': [
            'amd-0x6613',
            'amd-0x679e',
            'amd-0x67ef',
            'amd-0x6821',
            'amd-0x7340',
        ],
        'apple': [
            'apple-apple-m1',
            'apple-apple-m2',
            'apple-angle-metal-renderer:-apple-m1',
            'apple-angle-metal-renderer:-apple-m2',
        ],
        'arm': [],
        'google': [
            'google-0xffff',
            'google-0xc0de',
        ],
        'imagination': [],
        'intel': [
            # Individual GPUs should technically fit under intel-gen-X, but we
            # only support one level of nesting, so treat the generation tags as
            # individual GPUs.
            'intel-gen-9',
            'intel-gen-12',
            'intel-0xa2e',
            'intel-0xd26',
            'intel-0xa011',
            'intel-0x3e92',
            'intel-0x3e9b',
            'intel-0x4680',
            'intel-0x5912',
            'intel-0x9bc5',
        ],
        'nvidia': [
            'nvidia-0xfe9',
            'nvidia-0x1cb3',
            'nvidia-0x2184',
            'nvidia-0x2783',
        ],
        'qualcomm': [
            # 043a = 0x41333430 = older Adreno GPU
            # 0636 = 0x36333630 = Adreno 690 GPU (such as Surface Pro 9 5G)
            # 0c36 = 0x36334330 = Adreno 741 GPU
            'qualcomm-0x41333430',
            'qualcomm-0x36333630',
            'qualcomm-0x36334330',
        ],
    },
}


def _GenerateTagSpecializationStrings() -> Dict[str, str]:
  """Generates string a string representation of |TAG_SPECIALIZATIONS|.

  The resulting dictionary can be fed directly into string.format().

  Returns:
    A dict mapping tag_set_name to tag_set_string. |tag_set_string| is the
    formatted, expectation parser-compatible string for the information
    contained within TAG_SPECIALIZATIONS[tag_set_name].
  """
  tag_specialization_strings = {}
  for tag_set_name, tag_set in TAG_SPECIALIZATIONS.items():
    # Create an appropriately wrapped set of lines for each group, join them,
    # and add the necessary bits to make them a parseable tag set.
    wrapped_tag_lines = []
    num_groups = len(tag_set)
    current_group = 0
    for general_tag, specialized_tags in tag_set.items():
      current_group += 1
      wrapped_tag_lines.extend(
          _CreateWrappedLinesForTagGroup([general_tag] + specialized_tags,
                                         current_group == num_groups))

    wrapped_tags_string = '\n'.join(wrapped_tag_lines)
    tag_set_string = ''
    for i, line in enumerate(wrapped_tags_string.splitlines(True)):
      tag_set_string += LINE_START
      if i == 0:
        tag_set_string += TAG_SET_START
      else:
        tag_set_string += (' ' * len(TAG_SET_START))
      tag_set_string += line
    tag_set_string += TAG_SET_END
    tag_specialization_strings[tag_set_name] = tag_set_string
  return tag_specialization_strings


def _CreateWrappedLinesForTagGroup(tag_group: List[str],
                                   is_last_group: bool) -> List[str]:
  tag_line = ' '.join(tag_group)
  line_length = DEFAULT_LINE_LENGTH
  # If this will be the last group, we have to make sure we wrap such that
  # there will be enough room for the closing bracket of the tag set.
  if is_last_group:
    line_length -= len(TAG_SET_END)
  return textwrap.wrap(tag_line,
                       width=line_length,
                       subsequent_indent=BREAK_INDENTATION,
                       break_on_hyphens=False)


TAG_HEADER = """\
# OS
{OS_TAGS}
# Devices
# tags: [ android-nexus-5x android-pixel-2 android-pixel-4
#             android-pixel-6 android-shield-android-tv android-sm-a135m
#             android-sm-a235m android-sm-s911u1 android-moto-g-power-5g---2023
#         chromeos-board-amd64-generic chromeos-board-eve chromeos-board-jacuzzi
#             chromeos-board-octopus chromeos-board-volteer
#         fuchsia-board-astro fuchsia-board-nelson fuchsia-board-sherlock
#             fuchsia-board-qemu-x64 ]
# Platform
# tags: [ desktop
#         mobile ]
# Browser
{BROWSER_TAGS}
# GPU
{GPU_TAGS}
# Architecture
# tags: [ mac-arm64 mac-x86_64 ]
# Decoder
# tags: [ passthrough no-passthrough ]
# Browser Target CPU
# tags: [ target-cpu-64 target-cpu-32 target-cpu-31 ]
# ANGLE Backend
# tags: [ angle-disabled
#         angle-d3d9 angle-d3d11
#         angle-metal
#         angle-opengl angle-opengles
#         angle-swiftshader
#         angle-vulkan ]
# Skia Renderer
# tags: [ renderer-skia-gl
#         renderer-skia-vulkan
#         renderer-software ]
# Driver
# tags: [ mesa_lt_19.1
#         mesa_ge_21.0
#         mesa_ge_23.2
#         nvidia_ge_31.0.15.4601 nvidia_lt_31.0.15.4601
#         nvidia_ge_535.183.01 nvidia_lt_535.183.01 ]
# ASan
# tags: [ asan no-asan ]
# Display Server
# tags: [ display-server-wayland display-server-x ]
# OOP-Canvas
# tags: [ oop-c no-oop-c ]
# WebGPU Backend Validation
# tags: [ dawn-backend-validation dawn-no-backend-validation ]
# WebGPU Adapter
# tags: [ webgpu-adapter-default webgpu-adapter-swiftshader ]
# WebGPU DXC
# tags: [ webgpu-dxc-enabled webgpu-dxc-disabled ]
# WebGPU worker usage
# tags: [ webgpu-no-worker
#         webgpu-service-worker
#         webgpu-dedicated-worker
#         webgpu-shared-worker ]
# Clang coverage
# tags: [ clang-coverage no-clang-coverage ]
# Skia Graphite
# tags: [ graphite-enabled graphite-disabled ]
# results: [ Failure RetryOnFailure Skip Slow ]
""".format(**_GenerateTagSpecializationStrings())

TAG_HEADER_BEGIN =\
    '# BEGIN TAG HEADER (autogenerated, see validate_tag_consistency.py)'
TAG_HEADER_END = '# END TAG HEADER'

EXPECTATION_DIR = os.path.join(os.path.dirname(__file__), 'gpu_tests',
                               'test_expectations')


def Validate():
  retval = 0
  for f in (f for f in os.listdir(EXPECTATION_DIR) if f.endswith('.txt')):
    with open(os.path.join(EXPECTATION_DIR, f)) as infile:
      content = infile.read()
      start_index = content.find(TAG_HEADER_BEGIN)
      end_index = content.find(TAG_HEADER_END)
      if (start_index < 0 or end_index < 0
          or content[start_index + len(TAG_HEADER_BEGIN) + 1:end_index] !=
          TAG_HEADER):
        retval = 1
        logging.error(
            'Expectation file %s does not have a tag/result header consistent '
            'with the source of truth.', f)
  if retval:
    logging.error(
        'See %s for the expected header or run it in the "apply" mode to apply '
        'the source of truth to all expectation files.', __file__)
  return retval


def Apply():
  retval = 0
  for f in (f for f in os.listdir(EXPECTATION_DIR) if f.endswith('.txt')):
    filepath = os.path.join(EXPECTATION_DIR, f)
    with open(filepath) as infile:
      content = infile.read()
    start_index = content.find(TAG_HEADER_BEGIN)
    if start_index < 0:
      retval = 1
      logging.error(
          'Expectation file %s did not have tag header start string "%s".', f,
          TAG_HEADER_BEGIN)
      continue
    end_index = content.find(TAG_HEADER_END)
    if end_index < 0:
      retval = 1
      logging.error(
          'Expectation file %s did not have tag header end string "%s".', f,
          TAG_HEADER_END)
      continue
    content = (content[:start_index + len(TAG_HEADER_BEGIN)] + '\n' +
               TAG_HEADER + content[end_index:])
    with open(filepath, 'w') as outfile:
      outfile.write(content)
  return retval


def main():
  parser = argparse.ArgumentParser(
      description=('Validate that all test expectation tags are identical '
                   'across all expectation files or apply the source of truth '
                   'to all expectation files.'))
  parser.add_argument('function',
                      choices=['apply', 'validate'],
                      help='What the script should do.')
  args = parser.parse_args()
  if args.function == 'apply':
    return Apply()
  return Validate()


if __name__ == '__main__':
  sys.exit(main())
