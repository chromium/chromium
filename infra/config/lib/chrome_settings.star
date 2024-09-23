# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Library for configuring global defaults for CBI libs.

Generators can't access lucicfg vars, so in order to provide
configuration for generators, the information must be present in the
lucicfg graph. This file provides the chrome_settings struct that
can be used to configure project-wide defaults. For generators to access
this information, separate functions are provided for reading the
information from the graph.
"""

load("./nodes.star", "nodes")

_PER_BUILDER_OUTPUTS = nodes.create_singleton_node_type("per_builder_outputs")
_TARGETS = nodes.create_singleton_node_type("targets")

def _per_builder_outputs(*, root_dir):
    """Configure per-builder outputs for the project.

    Generators can get the config values using per_builder_outputs_config.

    Args:
        root_dir: The root directory in the generated output directory under
            which per-builder outputs will be generated. Must be a non-empty
            string.
    """
    if not root_dir or type(root_dir) != type(""):
        fail("root_dir")
    _PER_BUILDER_OUTPUTS.add(props = dict(
        root_dir = root_dir,
    ))

def per_builder_outputs_config():
    """Get the per-builder outputs config.

    Returns:
        A struct with the following attributes:
        * root_dir: The name of the root directory to generate
            per-builder outputs to.
    """
    node = _PER_BUILDER_OUTPUTS.get()
    if node == None:
        fail("In order to generate per-builder outputs, chrome_settings.per_builder_outputs must be called")
    return node.props

def _targets(*, autoshard_exceptions_file = None):
    """Configure targets for the project.

    Generators can get the config values using targets_config.

    Args:
        autoshard_exceptions_file: The path to the autoshard exceptions file,
            relative to the starlark root.
    """
    autoshard_exceptions = {}
    if autoshard_exceptions_file:
        raw_exceptions = json.decode(io.read_file(autoshard_exceptions_file))
        for builder_group, raw_exceptions_for_group in raw_exceptions.items():
            exceptions_for_group = autoshard_exceptions.setdefault(builder_group, {})
            for builder, raw_exceptions_for_builder in raw_exceptions_for_group.items():
                exceptions_for_builder = exceptions_for_group.setdefault(builder, {})
                for test, raw_exception_for_test in raw_exceptions_for_builder.items():
                    exceptions_for_builder[test] = int(raw_exception_for_test["shards"])

    _TARGETS.add(props = dict(
        autoshard_exceptions = autoshard_exceptions,
    ))

def targets_config():
    """Get the targets config.

    Returns:
        A struct with the following attributes:
        * autoshard_exceptions: A dict mapping
            builder group -> builder name -> test name -> shard count.
    """
    node = _TARGETS.get()
    if node == None:
        return struct(
            autoshard_exceptions = {},
        )
    return node.props

chrome_settings = struct(
    per_builder_outputs = _per_builder_outputs,
    targets = _targets,
)
