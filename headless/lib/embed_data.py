# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os

COPYRIGHT="""// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""

HEADER="""{copyright}

#include "base/containers/span.h"

namespace {namespace} {{

extern const base::span<const uint8_t> {variable_name};

}}  // namespace {namespace}
"""

SOURCE="""{copyright}

#include "{header_file}"

namespace {{

const uint8_t contents[] = {contents};

}}  // anonymous namespace

namespace {namespace} {{

const base::span<const uint8_t> {variable_name}(contents);

}}  // namespace {namespace}
"""

def ParseArguments(args):
  cmdline_parser = argparse.ArgumentParser()
  cmdline_parser.add_argument('--data_file', required=True)
  cmdline_parser.add_argument('--gendir', required=True)
  cmdline_parser.add_argument('--header_file', required=True)
  cmdline_parser.add_argument('--source_file', required=True)
  cmdline_parser.add_argument('--namespace', required=True)
  cmdline_parser.add_argument('--variable_name', required=True)

  return cmdline_parser.parse_args(args)


def GenerateArray(filepath):
  with open(filepath, 'rb') as f:
    contents = f.read()

  contents = [ str(byte) for byte in bytearray(contents) ]

  return '{' + ','.join(contents) + '}'


def GenerateHeader(args):
  return HEADER.format(
      copyright=COPYRIGHT,
      namespace=args.namespace,
      variable_name=args.variable_name)

def GenerateSource(args):
  contents = GenerateArray(args.data_file)

  return SOURCE.format(
      copyright=COPYRIGHT,
      header_file=args.header_file,
      namespace=args.namespace,
      contents=contents,
      variable_name=args.variable_name)


def WriteHeader(args):
  with open(os.path.join(args.gendir, args.header_file), 'w') as f:
    f.write(GenerateHeader(args))


def WriteSource(args):
  with open(os.path.join(args.gendir, args.source_file), 'w') as f:
    f.write(GenerateSource(args))


if __name__ == '__main__':
  args = ParseArguments(sys.argv[1:])

  WriteHeader(args)
  WriteSource(args)

