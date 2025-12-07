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

from typing import List, Set

BASE_FEATURE_PATTERN = br'BASE_FEATURE\((.*?)\);'
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
    'gpu',
    'headless',
    'infra',
    'internal',
    'ios',
    'ipc',
    'media',
    'mojo',
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


def _FindFeaturesInFile(filepath: str) -> List[str]:
  # Work on bytes to avoid utf-8 decode errors outside feature declarations
  file_contents = pathlib.Path(filepath).read_bytes()
  matches = BASE_FEATURE_RE.finditer(file_contents)
  feature_names = []
  for m in matches:
    # Split the arguments to handle both 2- and 3-argument versions of
    # BASE_FEATURE.
    args = [arg.strip() for arg in m.group(1).split(b',')]
    if len(args) == 3:
      # 3-arg: BASE_FEATURE(kMyFeature, "MyFeature", ...), name is the 2nd arg.
      feature_name = args[1].strip(b'"')
    elif len(args) == 2:
      # 2-arg: BASE_FEATURE(kMyFeature, ...)
      feature_name = args[0]
      if not feature_name.startswith(b'k'):
        continue
      feature_name = feature_name[1:]
    else:
      # Should not happen with valid C++ code.
      continue
    feature_names.append(feature_name.decode('utf-8'))
  return feature_names


def _FindDeclaredFeaturesImpl(repository_root: pathlib.Path) -> Set[str]:
  # Features are supposed to be defined in .cc files.
  # Iterate over the search folders in the root.
  root = pathlib.Path(repository_root)
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


def FindDeclaredFeatures(input_api) -> Set[str]:
  """Finds all declared feature names in the source code.

  This function will scan all *.cc and *.mm files and look for features
  defined with the BASE_FEATURE macro. It will extract the feature names.

  Args:
    input_api: InputApi instance for opening files
  Returns:
    Set of defined feature names in the source tree.
  """
  return _FindDeclaredFeaturesImpl(input_api.change.RepositoryRoot())


if __name__ == '__main__':
  print(_FindDeclaredFeaturesImpl(pathlib.Path('.')))
