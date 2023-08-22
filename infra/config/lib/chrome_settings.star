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
        fail("In order to generate per-builder outputs, project.per_builder_outputs must be called")
    return node.props

chrome_settings = struct(
    per_builder_outputs = _per_builder_outputs,
)
