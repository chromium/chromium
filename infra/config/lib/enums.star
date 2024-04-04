# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _enum(**kwargs):
    """Create an enum struct.

    Args:
        **kwargs - The enum values to create. A field will be added the struct
            with the key for the name of the field and the value for the value
            of the field. It is an error if there is an enum value named
            `values`.

    Returns:
        A struct with fields for each item in `kwargs`. The struct will also
        have a `values` field that contains the names of all the enum values.
        This allows for easily checking that a value is present in the enum.
    """
    if "values" in kwargs:
        fail("cannot create an enum value named 'values'")
    return struct(values = kwargs.values(), **kwargs)

enums = struct(
    enum = _enum,
)
