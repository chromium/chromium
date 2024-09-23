# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
    expected_path = 'third_party/blink/renderer/bindings/scripts/web_idl/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.abspath(
        os.path.join(this_dir, *(['..'] * expected_path.count('/'))))

    module_dirs = (
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'build',
                     'scripts'),
        # //third_party/ply
        os.path.join(root_dir, 'third_party'),
        # //third_party/pyjson5/src/json5
        os.path.join(root_dir, 'third_party', 'pyjson5', 'src'),
        # //tools/idl_parser
        os.path.join(root_dir, 'tools'),
    )
    for module_dir in reversed(module_dirs):
        # Preserve sys.path[0] as is.
        # https://docs.python.org/3/library/sys.html?highlight=path[0]#sys.path
        sys.path.insert(1, module_dir)


_setup_sys_path()

from . import file_io
from .argument import Argument
from .ast_group import AstGroup
from .async_iterator import AsyncIterator
from .attribute import Attribute
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Component
from .composition_parts import DebugInfo
from .composition_parts import Identifier
from .constant import Constant
from .constructor import Constructor
from .constructor import ConstructorGroup
from .database import Database
from .database_builder import build_database
from .dictionary import Dictionary
from .dictionary import DictionaryMember
from .enumeration import Enumeration
from .exposure import Exposure
from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributes
from .function_like import FunctionLike
from .function_like import OverloadGroup
from .idl_type import IdlType
from .interface import AsyncIterable
from .interface import IndexedAndNamedProperties
from .interface import Interface
from .interface import Iterable
from .interface import LegacyWindowAlias
from .interface import Maplike
from .interface import Setlike
from .interface import Stringifier
from .literal_constant import LiteralConstant
from .namespace import Namespace
from .observable_array import ObservableArray
from .operation import Operation
from .operation import OperationGroup
from .runtime_enabled_features import RuntimeEnabledFeatures
from .sync_iterator import SyncIterator
from .typedef import Typedef
from .union import Union


def init(runtime_enabled_features_paths):
    """
    Args:
        runtime_enabled_features_paths: Paths to the definition files of
            runtime-enabled features ("runtime_enabled_features.json5").
    """
    RuntimeEnabledFeatures.init(filepaths=runtime_enabled_features_paths)
