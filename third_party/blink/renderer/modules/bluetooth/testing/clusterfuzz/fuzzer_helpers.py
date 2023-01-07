# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module that includes classes and functions used by fuzzers."""


def FillInParameter(parameter, func, template):
    """Replaces occurrences of a parameter by calling a provided generator.

    Args:
      parameter: A string representing the parameter that should be replaced.
      func: A function that returns a string representing the value used to
          replace an instance of the parameter.
      template: A string that contains the parameter to be replaced.

    Returns:
      A string containing the value of |template| in which instances of
      |pameter| have been replaced by results of calling |func|.

    """
    result = template
    while parameter in result:
        result = result.replace(parameter, func(), 1)

    return result
