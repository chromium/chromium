# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python module to find feature names in source code.

These functions are declared in a separate module to allow multiprocessing to
correctly unpickle the called functions again.
"""

import glob
import itertools
import multiprocessing
import pathlib
import re

BASE_FEATURE_PATTERN = br'BASE_FEATURE\((.*?),(.*?),(.*?)\);'
BASE_FEATURE_RE = re.compile(BASE_FEATURE_PATTERN,
                             flags=re.MULTILINE + re.DOTALL)

# Only search these directories for flags. If your flag is outside these root
# directories, then add the directory here.
DIRECTORIES_TO_SEARCH = [
    'android_webview',
    'apps',
    'ash',
    'base',
    'cc',
    'chrome',
    'chromecast',
    'chromeos',
    'clank',
    'components',
    'content',
    'courgette',
    'crypto',
    'dbus',
    'device',
    'extensions',
    'fuchsia_web',
    'gin',
    'google_apis',
    'google_update',
    'gpu',
    'headless',
    'infra',
    'internal',
    'ios',
    'ipc',
    'media',
    'mojo',
    'native_client',
    'native_client_sdk',
    'net',
    'pdf',
    'ppapi',
    'printing',
    'remoting',
    'rlz',
    'sandbox',
    'services',
    'skia',
    'sql',
    'storage',
    # third_party/blink handled separately in FindDeclaredFeatures
    'ui',
    'url',
    'v8',
    'webkit',
    'weblayer',
]


def _FindFeaturesInFile(filepath):
  # Work on bytes to avoid utf-8 decode errors outside feature declarations
  file_contents = pathlib.Path(filepath).read_bytes()
  matches = BASE_FEATURE_RE.finditer(file_contents)
  # Remove whitespace and surrounding " from the second argument
  # which is the feature name.
  return [m.group(2).strip().strip(b'"').decode('utf-8') for m in matches]


def FindDeclaredFeatures(input_api):
  """Finds all declared feature names in the source code.

  This function will scan all *.cc and *.mm files and look for features
  defined with the BASE_FEATURE macro. It will extract the feature names.

  Args:
    input_api: InputApi instance for opening files
  Returns:
    Set of defined feature names in the source tree.
  """
  # Features are supposed to be defined in .cc files.
  # Iterate over the search folders in the root.
  root = pathlib.Path(input_api.change.RepositoryRoot())
  glob_patterns = [
      str(p / pathlib.Path('**/*.cc')) for p in root.iterdir()
      if p.is_dir() and p.name in DIRECTORIES_TO_SEARCH
  ]

  # blink is the only directory in third_party that should be searched.
  blink_glob = str(root / pathlib.Path('third_party/blink/**/*.cc'))
  glob_patterns.append(blink_glob)

  # Additional features for iOS can be found in mm files in the ios directory.
  mm_glob = str(root / pathlib.Path('ios/**/*.mm'))
  glob_patterns.append(mm_glob)

  # Create glob iterators that lazily go over the files to search
  glob_iterators = [
      glob.iglob(pattern, recursive=True) for pattern in glob_patterns
  ]

  # Limit to 4 processes - the disk accesses becomes a bottleneck with just a
  # few processes, but splitting the searching across multiple CPUs does yield
  # a benefit of a few seconds.
  # The exact batch size does not seem to matter much, as long as it is >> 1.
  pool = multiprocessing.Pool(4)
  found_features = pool.imap_unordered(_FindFeaturesInFile,
                                       itertools.chain(*glob_iterators), 1000)
  pool.close()
  pool.join()

  feature_names = set()
  for feature_list in found_features:
    feature_names.update(feature_list)
  return feature_names
