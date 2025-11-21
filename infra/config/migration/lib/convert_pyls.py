#! /usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections.abc
import datetime
import typing

from . import comments
from . import pyl
from . import starlark_conversions
from . import values

_COPYRIGHT_HEADER = f"""\
# Copyright {datetime.datetime.now().year} The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.\
"""

_TARGETS_LOAD = 'load("@chromium-luci//targets.star", "targets")'


def _file_content(
    docstring: str,
    declarations: collections.abc.Iterable[values.Value],
) -> str:
  content = [_COPYRIGHT_HEADER, f'"""{docstring}"""']
  add_load = True
  for declaration in declarations:
    declaration_text = values.to_output(declaration)
    assert declaration_text
    if add_load:
      content.append(_TARGETS_LOAD)
      add_load = False
    content.append(declaration_text)

  return '\n\n'.join(content)


_BINARIES_DOCSTRING = """\
Binary declarations

Binaries can be referenced by tests and define the label of the compile target
to be built as well as various aspects that the infrastructure needs to know in
order to run the binary.
"""

_COMPILE_TARGETS_DOCSTRING = """\
Compile target declarations

Compile targets can be referenced in additional_compile_targets for a builder in
waterfalls.pyl or as additional_compile_targets in a bundle declaration.
"""


def convert_gn_isolate_map_pyl(
    gn_isolate_map: pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
) -> dict[str, str]:
  binary_declarations = []
  compile_target_declarations = []

  for isolate_name, isolate in gn_isolate_map.items:
    for key, value in isolate.items:
      if key.value == 'type':
        isolate_type = typing.cast(pyl.Str, value).value
        break
    else:
      raise Exception(
          f'{isolate_name.start}: isolate {isolate_name.value} missing type')

    is_compile_target = isolate_type == 'additional_compile_target'

    function = ('targets.compile_target'
                if is_compile_target else f'targets.binaries.{isolate_type}')

    declaration = values.CallValueBuilder(function)
    # Comments will be attached to the declaration itself
    declaration['name'] = starlark_conversions.convert_direct(
        isolate_name, include_comments=False)

    for key, value in isolate.items:
      match key.value:
        case 'type':
          # Already handled by choosing the function to call
          continue

        case 'args' | 'module_scheme':
          if is_compile_target:
            raise Exception((f'{key.start}: args specified for isolate'
                             f' "{isolate_name.value}"'
                             ' with type "additional_compile_target"'))

        case 'label':
          pass

        case 'script':
          if isolate_type != 'script':
            raise Exception(
                f'{key.start}: script specified for isolate "{isolate_name.value}"'
                f' with non-"script" type "{isolate_type}"')

        case _:
          raise Exception(
              f'{key.start}: unhandled key in isolate: "{key.value}"')

      converted_value = starlark_conversions.convert_direct(value)
      assert not isinstance(converted_value, values.CommentedValue), (
          f'{value.start} unexpected comment on value for key "{key.value}"')
      converted_param = starlark_conversions.param_name(key)
      declaration[converted_param] = converted_value

    declaration = comments.comment(isolate_name, declaration)

    declarations_to_update = (compile_target_declarations
                              if is_compile_target else binary_declarations)
    declarations_to_update.append(declaration)

  binaries_content = _file_content(_BINARIES_DOCSTRING, binary_declarations)
  compile_targets_content = _file_content(_COMPILE_TARGETS_DOCSTRING,
                                          compile_target_declarations)
  return {
      'targets/binaries.star': binaries_content,
      'targets/compile_targets.star': compile_targets_content,
  }
