# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module to fuzz parameters of a template."""

import constraints
from fuzzer_helpers import FillInParameter


def FuzzParameters(test_file_data):
    """Fuzzes the data in the string provided.

    For now this function only replaces the TRANSFORM_SERVICE_UUID parameter
    with a valid UUID string.

    Args:
      test_file_data: String that contains parameters to be replaced.

    Returns:
      A string containing the value of test_file_data but with all its
      parameters replaced.
    """

    test_file_data = FillInParameter('TRANSFORM_BASIC_BASE',
                                     constraints.GetBasicBase, test_file_data)

    test_file_data = FillInParameter('TRANSFORM_DEVICE_DISCOVERY_BASE',
                                     constraints.GetDeviceDiscoveryBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_CONNECTABLE_BASE',
                                     constraints.GetConnectableBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_SERVICES_RETRIEVED_BASE',
                                     constraints.get_services_retrieved_base,
                                     test_file_data)

    test_file_data = FillInParameter(
        'TRANSFORM_CHARACTERISTICS_RETRIEVED_BASE',
        constraints.get_characteristics_retrieved_base, test_file_data)

    test_file_data = FillInParameter('TRANSFORM_REQUEST_DEVICE_OPTIONS',
                                     constraints.GetRequestDeviceOptions,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_GET_PRIMARY_SERVICES',
                                     constraints.get_get_primary_services_call,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_GET_CHARACTERISTICS',
                                     constraints.get_characteristics_call,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_PICK_A_SERVICE',
                                     constraints.get_pick_a_service,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_PICK_A_CHARACTERISTIC',
                                     constraints.get_pick_a_characteristic,
                                     test_file_data)

    test_file_data = FillInParameter(
        'TRANSFORM_VALUE', constraints.get_buffer_source, test_file_data)

    test_file_data = FillInParameter('TRANSFORM_RELOAD_ID',
                                     constraints.get_reload_id, test_file_data)

    return test_file_data
