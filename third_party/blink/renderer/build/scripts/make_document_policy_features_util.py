# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def parse_default_value(default_value,
                        value_type,
                        recognized_types=('Bool', 'DecDouble')):
    """ Parses default_value string to actual usable C++ expression.
    @param default_value_str: default_value field specified in document_policy_features.json5
    @param value_type: value_type field specified in document_policy_features.json5
    @param recognized_types: types that are valid for value_type
    @returns: a C++ expression that has type mojom::PolicyValue
    """
    if (value_type not in recognized_types):
        raise ValueError("{} is not recognized as valid value_type({})".format(
            value_type, recognized_types))

    policy_value_type = "mojom::PolicyValueType::k{}".format(value_type)

    if default_value == 'max':
        return "PolicyValue::CreateMaxPolicyValue({})".format(
            policy_value_type)
    if default_value == 'min':
        return "PolicyValue::CreateMinPolicyValue({})".format(
            policy_value_type)

    return "PolicyValue::Create{}({})".format(value_type, default_value)
