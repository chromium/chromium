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
        os.path.join(this_dir, *(['..'] * expected_path.count('/')))
    )

    module_dirs = (
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(
            root_dir, 'third_party', 'blink', 'renderer', 'build', 'scripts'
        ),
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

from . import file_io  # noqa: E402, F401
from .argument import Argument  # noqa: E402, F401
from .ast_group import AstGroup  # noqa: E402, F401
from .async_iterator import AsyncIterator  # noqa: E402, F401
from .attribute import Attribute  # noqa: E402, F401
from .callback_function import CallbackFunction  # noqa: E402, F401
from .callback_interface import CallbackInterface  # noqa: E402, F401
from .composition_parts import Component  # noqa: E402, F401
from .composition_parts import DebugInfo  # noqa: E402, F401
from .composition_parts import Identifier  # noqa: E402, F401
from .constant import Constant  # noqa: E402, F401
from .constructor import Constructor  # noqa: E402, F401
from .constructor import ConstructorGroup  # noqa: E402, F401
from .database import Database  # noqa: E402, F401
from .database_builder import build_database  # noqa: E402, F401
from .dictionary import Dictionary  # noqa: E402, F401
from .dictionary import DictionaryMember  # noqa: E402, F401
from .enumeration import Enumeration  # noqa: E402, F401
from .exposure import Exposure  # noqa: E402, F401
from .extended_attribute import ExtendedAttribute  # noqa: E402, F401
from .extended_attribute import ExtendedAttributes  # noqa: E402, F401
from .function_like import FunctionLike  # noqa: E402, F401
from .function_like import OverloadGroup  # noqa: E402, F401
from .idl_type import IdlType  # noqa: E402, F401
from .interface import AsyncIterable  # noqa: E402, F401
from .interface import IndexedAndNamedProperties  # noqa: E402, F401
from .interface import Interface  # noqa: E402, F401
from .interface import Iterable  # noqa: E402, F401
from .interface import LegacyWindowAlias  # noqa: E402, F401
from .interface import Maplike  # noqa: E402, F401
from .interface import Setlike  # noqa: E402, F401
from .interface import Stringifier  # noqa: E402, F401
from .literal_constant import LiteralConstant  # noqa: E402, F401
from .namespace import Namespace  # noqa: E402, F401
from .observable_array import ObservableArray  # noqa: E402, F401
from .operation import Operation  # noqa: E402, F401
from .operation import OperationGroup  # noqa: E402, F401
from .runtime_enabled_features import RuntimeEnabledFeatures  # noqa: E402
from .sync_iterator import SyncIterator  # noqa: E402, F401
from .typedef import Typedef  # noqa: E402, F401
from .union import Union  # noqa: E402, F401


def init(runtime_enabled_features_paths):
    """
    Args:
        runtime_enabled_features_paths: Paths to the definition files of
            runtime-enabled features ("runtime_enabled_features.json5").
    """
    RuntimeEnabledFeatures.init(filepaths=runtime_enabled_features_paths)
