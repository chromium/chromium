# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Node types internal to the targets lib.

Documentation about the nodes types can be found in ./README.md, labeled with
the node kind.
"""

load("//lib/nodes.star", _nodes_lib = "nodes")

# A target that can be built to run a test
_BINARY = _nodes_lib.create_unscoped_node_type("targets|binary")

# A mapping from the ninja target name to GN label and associated details.
_LABEL_MAPPING = _nodes_lib.create_unscoped_node_type("targets|label-mapping")

# A set of modifications to make when expanding tests in a suite
_MIXIN = _nodes_lib.create_unscoped_node_type("targets|mixin", allow_empty_id = True)

# A set of modifications to make when multiply expanding a test in a matrix
# compound suite
_VARIANT = _nodes_lib.create_unscoped_node_type("targets|variant")

# A test that can be included in a basic suite
_LEGACY_TEST = _nodes_lib.create_unscoped_node_type("targets|legacy-test")

# A basic suite, which is a set of tests with optional modifications
_LEGACY_BASIC_SUITE = _nodes_lib.create_unscoped_node_type("targets|legacy-basic-suite")

# Modifications to apply to a test included in a basic suite
_LEGACY_BASIC_SUITE_CONFIG = _nodes_lib.create_scoped_node_type("targets|legacy-basic-suite-config", _LEGACY_BASIC_SUITE.kind)

# A mixin to remove from a test included in a basic suite.
_LEGACY_BASIC_SUITE_REMOVE_MIXIN = _nodes_lib.create_link_node_type("targets|legacy-remove-mixin", _LEGACY_BASIC_SUITE_CONFIG, _MIXIN)

# A compound suite, which is a set of basic suites
_LEGACY_COMPOUND_SUITE = _nodes_lib.create_unscoped_node_type("targets|legacy-compound-suite")

# A matrix compound suite, which is a set of basic suites that can be optionally
# expanded with multiple variants
_LEGACY_MATRIX_COMPOUND_SUITE = _nodes_lib.create_unscoped_node_type("targets|legacy-matrix-compound-suite")

# The modifications to apply to tests in a basic suite included in a matrix
# compound suite
_LEGACY_MATRIX_CONFIG = _nodes_lib.create_scoped_node_type("targets|legacy-matrix-config", _LEGACY_MATRIX_COMPOUND_SUITE.kind)

# Compile targets, which can be specified as additional compile targets in a
# bundle
_COMPILE_TARGET = _nodes_lib.create_unscoped_node_type("targets|compile-target")

# A test target that can be included in a bundle used by builders that have
# their targets defined in starlark
_TEST = _nodes_lib.create_unscoped_node_type("targets|test")

# A collection of compile targets to build and tests to run with optional
# modifications
_BUNDLE = _nodes_lib.create_unscoped_node_type("targets|bundle", allow_empty_id = True)

# Modifications to make to a single test contained in a bundle
_PER_TEST_MODIFICATION = _nodes_lib.create_scoped_node_type("targets|per-test-modification", _BUNDLE.kind)

# A mixin to ignore when expanding tests.
_REMOVE_MIXIN = _nodes_lib.create_link_node_type("targets|legacy-remove-mixin", _PER_TEST_MODIFICATION, _MIXIN)

nodes = struct(
    BINARY = _BINARY,
    LABEL_MAPPING = _LABEL_MAPPING,
    MIXIN = _MIXIN,
    VARIANT = _VARIANT,
    LEGACY_TEST = _LEGACY_TEST,
    LEGACY_BASIC_SUITE = _LEGACY_BASIC_SUITE,
    LEGACY_BASIC_SUITE_CONFIG = _LEGACY_BASIC_SUITE_CONFIG,
    LEGACY_BASIC_SUITE_REMOVE_MIXIN = _LEGACY_BASIC_SUITE_REMOVE_MIXIN,
    LEGACY_COMPOUND_SUITE = _LEGACY_COMPOUND_SUITE,
    LEGACY_MATRIX_COMPOUND_SUITE = _LEGACY_MATRIX_COMPOUND_SUITE,
    LEGACY_MATRIX_CONFIG = _LEGACY_MATRIX_CONFIG,
    COMPILE_TARGET = _COMPILE_TARGET,
    TEST = _TEST,
    BUNDLE = _BUNDLE,
    PER_TEST_MODIFICATION = _PER_TEST_MODIFICATION,
    REMOVE_MIXIN = _REMOVE_MIXIN,
)
