# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class RuleBase(object):
    """
    A base class that represents a rule for validations.
    Subclasses need to implement `is_valid` and `error_message`.
    """

    def is_valid(self, target):
        """
        Returns True if `target` satisfies this rule,
        or False if `target` violates this rule.

        `target` is an object included in web_idl.Database.
        Example: web_idl.Interface, web_idl.FunctionLike, etc.
        """
        raise NotImplementedError()

    @property
    def error_message(self):
        """
        If is_valid() returns False, then developers get this `error_message`.
        """
        raise NotImplementedError()
