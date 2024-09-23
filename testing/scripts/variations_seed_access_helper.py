# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared helper functions for r/w of variations seed in local state."""

import json
import logging
import os

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(_THIS_DIR, os.path.pardir, os.path.pardir)

# Constants around the Local State file and variation keys.
_LOCAL_STATE_FILE_NAME = 'Local State'
LOCAL_STATE_SEED_NAME = 'variations_compressed_seed'
LOCAL_STATE_SEED_SIGNATURE_NAME = 'variations_seed_signature'


def load_test_seed_from_file(hardcoded_seed_path):
  """Reads and parses the test variations seed.

  There are 2 types of seeds used by this smoke test:
  1. A provided seed under test, and when the test is running with this seed,
     it's running as a TRY job and is triggered by the finch_smoke_test recipe
     to test the Finch GCL config changes. The interface between the recipe and
     this test is a json file named variations_seed located at the root of
     the checkout.
  2. A hard-coded seed, and when the test is running with this seed, it's
     running on CI continuously to prevent regressions to this test itself.

  The function tries to use provided seed first. If the provided seed doesn't
  exist, the function will fallback to a hard-coded seed as input arg.

  Args:
    seed_json_path (str): Path to provided hard-coded seed.

  Returns:
    A tuple of two strings: the compressed seed and the seed signature.
  """
  # Provided seed path.
  path_seed = get_test_seed_file_path(hardcoded_seed_path)

  logging.info('Parsing test seed from "%s"', path_seed)

  with open(path_seed, 'r') as f:
    seed_json = json.load(f)

  return (seed_json.get(LOCAL_STATE_SEED_NAME, None),
          seed_json.get(LOCAL_STATE_SEED_SIGNATURE_NAME, None))


def get_test_seed_file_path(hardcoded_seed_path):
  """Gets the file path to the test seed.

  There are 2 types of seeds used by this smoke test:
  1. A provided seed under test, and when the test is running with this seed,
     it's running as a TRY job and is triggered by the finch_smoke_test recipe
     to test the Finch GCL config changes. The interface between the recipe and
     this test is a json file named variations_seed located at the root of
     the checkout.
  2. A hard-coded seed, and when the test is running with this seed, it's
     running on CI continuously to prevent regressions to this test itself.

  The function tries to use provided seed first. If the provided seed doesn't
  exist, the function will fallback to a hard-coded seed as input arg.

  Args:
    hardcoded_seed_path (str): Path to provided hard-coded seed.
  Returns:
    A path to the location of the seed.
  """
  path_seed = os.path.abspath(os.path.join(_SRC_DIR, 'variations_seed'))

  if not os.path.isfile(path_seed):
    path_seed = hardcoded_seed_path

  return path_seed


def get_current_seed(user_data_dir):
  """Gets the current seed.

  Args:
    user_data_dir (str): Path to the user data directory used to launch Chrome.

  Returns:
    A tuple of two strings: the compressed seed and the seed signature.
  """
  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME)) as f:
    local_state = json.load(f)

  return local_state.get(LOCAL_STATE_SEED_NAME,
                         None), local_state.get(LOCAL_STATE_SEED_SIGNATURE_NAME,
                                                None)


def update_local_state(user_data_dir, update_dict):
  """Updates local state file under user data dir with given dict.

  Args:
    user_data_dir (str): Path to the user data directory used to launch Chrome.
    update_dict (dict): A dict used to update current local state file.
  """
  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME)) as f:
    local_state = json.load(f)

  local_state.update(update_dict)

  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME), 'w') as f:
    json.dump(local_state, f)


def inject_test_seed(seed, signature, user_data_dir):
  """Injects the given test seed.

  Args:
    seed (str): A variations seed.
    signature (str): A seed signature.
    user_data_dir (str): Path to the user data directory used to launch Chrome.

  Returns:
    bool: Whether the injection succeeded.
  """
  seed_dict = {
      LOCAL_STATE_SEED_NAME: seed,
      LOCAL_STATE_SEED_SIGNATURE_NAME: signature,
  }
  update_local_state(user_data_dir, seed_dict)
  current_seed, current_signature = get_current_seed(user_data_dir)
  if current_seed != seed or current_signature != signature:
    return False
  return True
