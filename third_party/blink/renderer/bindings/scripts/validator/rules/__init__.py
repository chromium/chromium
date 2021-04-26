# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import rules_function_like
from . import rules_extended_attributes


def register_all_rules(rule_store):
    rules_function_like.register_rules(rule_store)
    rules_extended_attributes.register_rules(rule_store)
