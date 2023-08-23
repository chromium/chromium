# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import rules_attributes
from . import rules_constants
from . import rules_dictionaries
from . import rules_extended_attributes
from . import rules_function_like
from . import rules_idl_types
from . import rules_observable_arrays


def register_all_rules(rule_store):
    rules_attributes.register_rules(rule_store)
    rules_constants.register_rules(rule_store)
    rules_dictionaries.register_rules(rule_store)
    rules_extended_attributes.register_rules(rule_store)
    rules_function_like.register_rules(rule_store)
    rules_idl_types.register_rules(rule_store)
    rules_observable_arrays.register_rules(rule_store)
