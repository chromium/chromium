# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for working with dimensions."""

def _dimensions(**kwargs):
    """Get a dict that can be used where dimensions are expected.

    Args:
        **kwargs: The dimensions to be included. If the dimension value is a
            struct, it must have a get_dimension attribute which can be called
            with the bucket and builder as positional arguments to get the
            actual dimension value.

    Returns:
        A struct with a resolve attribute that can be called with the bucket and
        builder as positional arguments to get dict of dimension values.
    """

    def resolve(bucket, builder):
        def to_dimension(val):
            if type(val) == type(struct()):
                val = val.get_dimension(bucket, builder)
            if val == False:
                val = 0
            elif val == True:
                val = 1
            return str(val)

        return {k: to_dimension(v) for k, v in kwargs.items() if v != None}

    return struct(
        resolve = resolve,
    )

dimensions = struct(
    dimensions = _dimensions,
)
