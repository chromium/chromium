# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class RuleBase(object):
    """
    A base class that represents a rule for validations.
    Subclasses need to implement `validate`.
    """

    def validate(self, assert_, target):
        """
        Validates that `target` satisfies the rule.

        Args:
          assert_:
            A function which takes a condition and a string error message.
          target:
            An object included in web_idl.Database.
        """
        raise NotImplementedError()
