# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module that contains information about Web Bluetooth's Fake Adapters."""

BLOCKLISTED_UUID = '611c954a-263b-4f4a-aab6-01ddb953f985'
DISCONNECTION_UUID = '01d7d889-7451-419f-aeb8-d65e7b9277af'
GATT_ERROR_UUID = '000000a0-97e5-4cd7-b9f1-f5a427670c59'

CONNECTION_ERROR_UUIDS = [
    '00000000-97e5-4cd7-b9f1-f5a427670c59',
    '00000001-97e5-4cd7-b9f1-f5a427670c59',
    '00000002-97e5-4cd7-b9f1-f5a427670c59',
    '00000003-97e5-4cd7-b9f1-f5a427670c59',
    '00000004-97e5-4cd7-b9f1-f5a427670c59',
    '00000005-97e5-4cd7-b9f1-f5a427670c59',
    '00000006-97e5-4cd7-b9f1-f5a427670c59',
    '00000007-97e5-4cd7-b9f1-f5a427670c59',
    '00000008-97e5-4cd7-b9f1-f5a427670c59',
    '00000009-97e5-4cd7-b9f1-f5a427670c59',
    '0000000a-97e5-4cd7-b9f1-f5a427670c59',
    '0000000b-97e5-4cd7-b9f1-f5a427670c59',
    '0000000c-97e5-4cd7-b9f1-f5a427670c59',
    '0000000d-97e5-4cd7-b9f1-f5a427670c59',
    '0000000e-97e5-4cd7-b9f1-f5a427670c59',
]

# List of services that are included in our fake adapters.
ADVERTISED_SERVICES = [
    'generic_access',
    'glucose',
    'tx_power',
    'heart_rate',
    'human_interface_device',
    'device_information',
    'a_device_name_that_is_longer_than_29_bytes_but_shorter_than_248_bytes',
    BLOCKLISTED_UUID,
    CONNECTION_ERROR_UUIDS[0],
    DISCONNECTION_UUID,
    GATT_ERROR_UUID,
]

# List of services inside devices.
SERVICES = [
    'generic_access',
    'heart_rate',
    'device_information',
    'generic_access',
    'heart_rate',
    'human_interface_device',
    'a_device_name_that_is_longer_than_29_bytes_but_shorter_than_248_bytes',
    BLOCKLISTED_UUID,
    DISCONNECTION_UUID,
    GATT_ERROR_UUID,
]

# List of characteristics inside devices.
CHARACTERISTICS = [
    'bad1c9a2-9a5b-4015-8b60-1579bbbf2135',
    '01d7d88a-7451-419f-aeb8-d65e7b9277af',
    'body_sensor_location',
    'gap.device_name',
    'heart_rate_measurement',
    'serial_number_string',
    'gap.peripheral_privacy_flag',
]

# Tuples of common service uuid and their characteristics uuids.
GENERIC_ACCESS_SERVICE = ('generic_access',
                          ['gap.device_name', 'gap.peripheral_privacy_flag'])

HEART_RATE_SERVICE = ('heart_rate',
                      ['heart_rate_measurement', 'body_sensor_location'])

# List of available fake adapters.
ALL_ADAPTERS = [
    'NotPresentAdapter',
    'NotPoweredAdapter',
    'EmptyAdapter',
    'FailStartDiscoveryAdapter',
    'GlucoseHeartRateAdapter',
    'UnicodeDeviceAdapter',
    'MissingServiceHeartRateAdapter',
    'MissingCharacteristicHeartRateAdapter',
    'HeartRateAdapter',
    'TwoHeartRateServicesAdapter',
    'DisconnectingHeartRateAdapter',
    'BlocklistTestAdapter',
    'FailingConnectionsAdapter',
    'FailingGATTOperationsAdapter',
    'DelayedServicesDiscoveryAdapter',
    'DeviceNameLongerThan29BytesAdapter',
]

# List of fake adapters that include devices.
ADAPTERS_WITH_DEVICES = [
    (
        'GlucoseHeartRateAdapter',
        ['generic_access', 'heart_rate', 'glucose', 'tx_power'],
    ),
    (
        'MissingServiceHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'MissingCharacteristicHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'HeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'TwoHeartRateServicesAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'DisconnectingHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'BlocklistTestAdapter',
        [
            BLOCKLISTED_UUID, 'device_information', 'generic_access',
            'heart_rate', 'human_interface_device'
        ],
    ),
    (
        'FailingConnectionsAdapter',
        CONNECTION_ERROR_UUIDS,
    ),
    (
        'FailingGATTOperationsAdapter',
        [GATT_ERROR_UUID],
    ),
    (
        'DelayedServicesDiscoveryAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'DeviceNameLongerThan29BytesAdapter',
        [
            'a_device_name_that_is_longer_than_29_bytes_but_shorter_than_248_bytes'
        ],
    ),
]

# List of fake adapters with services.
ADAPTERS_WITH_SERVICES = [
    (
        'MissingCharacteristicHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'HeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'TwoHeartRateServicesAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'DisconnectingHeartRateAdapter',
        ['generic_access', 'heart_rate', DISCONNECTION_UUID],
    ),
    (
        'BlocklistTestAdapter',
        [
            BLOCKLISTED_UUID, 'device_information', 'generic_access',
            'heart_rate', 'human_interface_device'
        ],
    ),
    (
        'FailingGATTOperationsAdapter',
        [GATT_ERROR_UUID],
    ),
    ('DelayedServicesDiscoveryAdapter', ['heart_rate']),
]

ADAPTERS_WITH_CHARACTERISTICS = [
    (
        'HeartRateAdapter',
        [GENERIC_ACCESS_SERVICE, HEART_RATE_SERVICE],
    ),
    (
        'TwoHeartRateServicesAdapter',
        [HEART_RATE_SERVICE],
    ),
    ('DisconnectingHeartRateAdapter', [
        GENERIC_ACCESS_SERVICE, HEART_RATE_SERVICE,
        (DISCONNECTION_UUID, ['01d7d88a-7451-419f-aeb8-d65e7b9277af'])
    ]),
    ('BlocklistTestAdapter', [
        GENERIC_ACCESS_SERVICE, HEART_RATE_SERVICE,
        (
            BLOCKLISTED_UUID,
            ['bad1c9a2-9a5b-4015-8b60-1579bbbf2135'],
        ), (
            'device_information',
            ['serial_number_string'],
        )
    ]),
    (
        'FailingGATTOperationsAdapter',
        [(GATT_ERROR_UUID, [
            '000000a1-97e5-4cd7-b9f1-f5a427670c59',
            '000000a2-97e5-4cd7-b9f1-f5a427670c59',
            '000000a3-97e5-4cd7-b9f1-f5a427670c59',
            '000000a4-97e5-4cd7-b9f1-f5a427670c59',
            '000000a5-97e5-4cd7-b9f1-f5a427670c59',
            '000000a6-97e5-4cd7-b9f1-f5a427670c59',
            '000000a7-97e5-4cd7-b9f1-f5a427670c59',
            '000000a8-97e5-4cd7-b9f1-f5a427670c59',
        ])],
    ),
]
