# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

def _CheckSphinxBuild(input_api, output_api):
  """Check that the docs are buildable without any warnings.

  This check runs sphinx-build with -W so that warning are errors.

  However, since the trybots don't have sphinx installed, we'll treat a sphinx
  failure as a warning. (Let's trust that the docs editors are testing locally.)
  """

  try:
    subprocess.check_output(['make', 'presubmit', 'SPHINXOPTS=-Wa'],
                            stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    return [output_api.PresubmitNotifyResult('sphinx_build failed:\n' +
                                             e.output.decode('utf-8'))]
  # FileNotFoundError is typically the exception thrown on Windows, because make
  # will usually not be in the path.
  except FileNotFoundError as e:
    return [output_api.PresubmitNotifyResult('sphinx_build failed')]

  return []


def CommonChecks(input_api, output_api):
  output = []
  output.extend(_CheckSphinxBuild(input_api, output_api))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
