# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
    expected_path = 'third_party/blink/renderer/bindings/scripts/web_idl/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.join(this_dir, *(['..'] * expected_path.count('/')))
    sys.path = [
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'build',
                     'scripts'),
        # //third_party/ply
        os.path.join(root_dir, 'third_party'),
        # //third_party/pyjson5/src/json5
        os.path.join(root_dir, 'third_party', 'pyjson5', 'src'),
        # //tools/idl_parser
        os.path.join(root_dir, 'tools'),
    ] + sys.path


_setup_sys_path()


from .ast_group import AstGroup
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Component
from .database import Database
from .database_builder import build_database
from .dictionary import Dictionary
from .enumeration import Enumeration
from .idl_type import IdlType
from .interface import Interface
from .namespace import Namespace
from .runtime_enabled_features import RuntimeEnabledFeatures
from .typedef import Typedef
from .union import Union


def init(runtime_enabled_features_paths):
    """
    Args:
        runtime_enabled_features_paths: Paths to the definition files of
            runtime-enabled features ("runtime_enabled_features.json5").
    """
    RuntimeEnabledFeatures.init(filepaths=runtime_enabled_features_paths)
