# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module to get random numbers, strings, etc.

   The values returned by the various functions can be replaced in
   templates to generate test cases.
"""

import math
import random
import sys
import uuid

# This script needs the utils.py and fuzzy_types.py modules in order
# to work. This files are copied by the setup.py script and not checked-in
# next to this code, so we need to disable the style warning.
# pylint: disable=F0401
from resources import utils
from resources import fuzzy_types

import gatt_aliases
import wbt_fakes

# Strings that are used to generate the beginning of a test. The replacement
# fields are replaced by Get*Base() functions below to generate valid test
# cases.
BASIC_BASE = \
    '  return setBluetoothFakeAdapter({fake_adapter_name})\n'\
    '    .then(() => {{\n'

DEVICE_DISCOVERY_BASE = BASIC_BASE + \
    '      return requestDeviceWithKeyDown({{\n'\
    '        filters: [{{services: [{service_uuid}]}}]}});\n'\
    '    }})\n'\
    '    .then(device => {{\n'

CONNECTABLE_BASE = DEVICE_DISCOVERY_BASE + \
    '      return device.gatt.connect();\n'\
    '    }})\n'\
    '    .then(gatt => {{\n'

SERVICE_RETRIEVED_BASE = CONNECTABLE_BASE + \
    '      return gatt.getPrimaryService({service_uuid});\n'\
    '    }})\n'\
    '    .then(services => {{\n'

SERVICES_RETRIEVED_BASE = CONNECTABLE_BASE + \
    '      return gatt.getPrimaryServices({optional_service_uuid});\n'\
    '    }})\n'\
    '    .then(services => {{\n'

CHARACTERISTIC_RETRIEVED_BASE = \
    '      TRANSFORM_PICK_A_SERVICE;\n'\
    '      return service.getCharacteristic({characteristic_uuid});\n'\
    '    }})\n'\
    '    .then(characteristics => {{\n'

CHARACTERISTICS_RETRIEVED_BASE = \
    '      TRANSFORM_PICK_A_SERVICE;\n'\
    '      return service.getCharacteristics({optional_characteristic_uuid});\n'\
    '    }})\n'\
    '    .then(characteristics => {{\n'


def _ToJsStr(s):
    return u'\'{}\''.format(s)


def _get_random_number():
    return utils.UniformExpoInteger(0, sys.maxsize.bit_length() + 1)


def _GetFuzzedJsString(s):
    """Returns a fuzzed string based on |s|.

    Args:
      s: The base string to fuzz.
    Returns:
      A single line string surrounded by quotes.
    """

    def _PostProcFuzzyStr(fuzzed):
        # Escape 'escape' characters.
        fuzzed = fuzzed.replace('\\', r'\\')
        # Escape quote characters.
        fuzzed = fuzzed.replace('\'', r'\'')
        # Put everything in a single line.
        fuzzed = '\\n'.join(fuzzed.split())
        return _ToJsStr(fuzzed)

    while True:
        fuzzed_string = fuzzy_types.FuzzyString(s)
        if isinstance(fuzzed_string, bytes):
            try:
                fuzzed_string = fuzzed_string.decode('utf-8')
            except UnicodeDecodeError:
                print("Can't decode fuzzed bytes. Trying again.")
                continue
            else:
                return _PostProcFuzzyStr(fuzzed_string)
        else:
            assert isinstance(fuzzed_string, str)
            return _PostProcFuzzyStr(fuzzed_string)


def _get_array_of_random_ints(max_length, max_value):
    """Returns an string with an array of random integer."""
    length = utils.UniformExpoInteger(0, math.log(max_length, 2))
    exp_max_value = math.log(max_value, 2)
    return '[{}]'.format(', '.join(
        str(utils.UniformExpoInteger(0, exp_max_value))
        for _ in range(length)))


def _get_typed_array():
    """Generates a TypedArray constructor.

    There are nine types of TypedArrays and TypedArray has four constructors.

    Types:
      * Int8Array
      * Int16Array
      * Int32Array
      * Uint8Array
      * Uint16Array
      * Uint32Array
      * Uint8ClampedArray
      * Float32Array
      * Float64Array

    Constructors:

     * new TypedArray(length)
     * new TypedArray(typedArray)
     * new TypedArray(object)
     * new TypedArray(buffer)

    Returns:
      A string made up of a randomly chosen type and argument type from the
      lists above.
    """
    array_type = random.choice([
        'Int8Array', 'Int16Array', 'Int32Array', 'Uint8Array', 'Uint16Array',
        'Uint32Array', 'Uint8ClampedArray', 'Float32Array', 'Float64Array'
    ])

    # Choose an argument type at random.
    arguments = random.choice([
        # length e.g. 293
        # We choose 2**10 as the upper boundry because the max length allowed
        # by WebBluetooth is 2**10.
        lambda: utils.UniformExpoInteger(0, 10),
        # typedArray e.g. new Uint8Array([1,2,3])
        _get_typed_array,
        # object e.g. [1,2,3]
        lambda: _get_array_of_random_ints(max_length=1000, max_value=2**64),
        # buffer e.g. new Uint8Array(10).buffer
        lambda: _get_typed_array() + '.buffer',
    ])

    return 'new {array_type}({arguments})'.format(
        array_type=array_type, arguments=arguments())


def GetAdvertisedServiceUUIDFromFakes():
    """Returns a random service string from the list of fake services."""
    return _ToJsStr(random.choice(wbt_fakes.ADVERTISED_SERVICES))


def get_service_uuid_from_fakes():
    """Returns a random service string from a list of fake services."""
    return _ToJsStr(random.choice(wbt_fakes.SERVICES))


def get_characteristic_uuid_from_fakes():
    """Returns a random characteristic string from a fake characteristics list."""
    return _ToJsStr(random.choice(wbt_fakes.CHARACTERISTICS))


def GetValidServiceAlias():
    """Returns a valid service alias from the list of services aliases."""
    return _ToJsStr(random.choice(gatt_aliases.SERVICES))


def get_valid_characteristic_alias():
    """Returns a valid service alias from the list of services aliases."""
    return _ToJsStr(random.choice(gatt_aliases.CHARACTERISTICS))


def GetRandomUUID():
    """Returns a random UUID, a random number or a fuzzed uuid or alias."""
    choice = random.choice(['uuid', 'number', 'fuzzed string'])
    if choice == 'uuid':
        return _ToJsStr(uuid.uuid4())
    elif choice == 'number':
        return _get_random_number()
    elif choice == 'fuzzed string':
        choice2 = random.choice(['uuid', 'alias'])
        if choice2 == 'uuid':
            random_uuid = str(uuid.uuid4())
            return _GetFuzzedJsString(random_uuid)
        elif choice2 == 'alias':
            alias = random.choice(gatt_aliases.SERVICES)
            return _GetFuzzedJsString(alias)


def GetAdvertisedServiceUUID():
    """Generates a random Service UUID from a set of functions.

    See get_service_uuid_from_fakes(), GetValidServiceAlias() and GetRandomUUID()
    for the different values this function can return.

    This function weights get_service_uuid_from_fakes() more heavily to increase the
    probability of generating test pages that can interact with the fake
    adapters.

    Returns:
      A string or a number that can be used as a Service UUID by the Web
      Bluetooth API.
    """
    roll = random.random()
    if roll < 0.8:
        return GetAdvertisedServiceUUIDFromFakes()
    elif roll < 0.9:
        return GetValidServiceAlias()
    else:
        return GetRandomUUID()


def get_service_uuid():
    """Generates a random Service UUID from a set of functions.

    Similar to GetAdvertisedServiceUUID() but weights get_service_uuid_from_fakes()
    more heavily to increase the  probability of generating test pages that can
    interact with the fake adapters.

    See get_service_uuid_from_fakes(), GetValidServiceAlias() and
    GetRandomUUID() for the different values this function can return.

    Returns:
      A string or a number that can be used as a Service UUID by the Web
      Bluetooth API.
    """
    roll = random.random()
    if roll < 0.8:
        return get_service_uuid_from_fakes()
    elif roll < 0.9:
        return GetValidServiceAlias()
    else:
        return GetRandomUUID()


def get_characteristic_uuid():
    """Generates a random Characteristic UUID from a set of functions.

    Similar to get_service_uuid() but weights get_characteristic_uuid_from_fakes()
    more heavily to increase the  probability of generating test pages that can
    interact with the fake adapters.

    See get_characteristic_uuid_from_fakes(), get_valid_characteristic_alias() and
    GetRandomUUID() for the different values this function can return.

    Returns:
      A string or a number that can be used as a Service UUID by the Web
      Bluetooth API.
    """
    roll = random.random()
    if roll < 0.8:
        return get_characteristic_uuid_from_fakes()
    elif roll < 0.9:
        return get_valid_characteristic_alias()
    else:
        return GetRandomUUID()


def GetRequestDeviceOptions():
    """Returns an object used by navigator.bluetooth.requestDevice."""
    # TODO(ortuno): Randomize the members, number of filters, services, etc.

    return '{filters: [{services: [ %s ]}]}' % GetAdvertisedServiceUUID()


def GetBasicBase():
    """Returns a string that sets a random fake adapter."""
    adapter = _ToJsStr(random.choice(wbt_fakes.ALL_ADAPTERS))
    return BASIC_BASE.format(fake_adapter_name=adapter)


def GetDeviceDiscoveryBase():
    """Generates a string that contains all steps to find a device."""
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_DEVICES)
    return DEVICE_DISCOVERY_BASE.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=_ToJsStr(random.choice(services)))


def GetConnectableBase():
    """Generates a string that contains all steps to connect to a device.

    Returns: A string that:
      1. Sets an adapter to a fake adapter with a connectable device.
      2. Looks for the connectable device.
      3. Connects to it.
    """
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_DEVICES)
    return DEVICE_DISCOVERY_BASE.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=_ToJsStr(random.choice(services)))


def get_services_retrieved_base():
    """Returns a string that contains all steps to retrieve a service.

    Returns: A string that:
      1. Sets an adapter to a fake adapter with a connectable device with
         services.
      2. Use one of the device's services to look for that device.
      3. Connects to it.
      4. Retrieve the device's service used in 2.
    """
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_SERVICES)
    service_uuid = _ToJsStr(random.choice(services))

    base = random.choice([SERVICE_RETRIEVED_BASE, SERVICES_RETRIEVED_BASE])
    return base.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=service_uuid,
        optional_service_uuid=random.choice(['', service_uuid]))


def get_characteristics_retrieved_base():
    """Returns a string that contains all steps to retrieve a characteristic.

      Returns: A string that:
        1. Sets an adapter to a fake adapter with a connectable device with
           services.
        2. Use one of the device's services to look for that device.
        3. Connects to it.
        4. Retrieve the device's service used in 2.
        5. Retrieve a characteristic from that service.
    """
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_CHARACTERISTICS)

    service_uuid, characteristics = random.choice(services)
    service_uuid = _ToJsStr(service_uuid)

    characteristic_uuid = _ToJsStr(random.choice(characteristics))

    optional_service_uuid = random.choice(['', service_uuid])
    optional_characteristic_uuid = random.choice(['', characteristic_uuid])

    services_base = random.choice(
        [SERVICE_RETRIEVED_BASE, SERVICES_RETRIEVED_BASE])

    characteristics_base = services_base + random.choice([
        CHARACTERISTIC_RETRIEVED_BASE,
        CHARACTERISTICS_RETRIEVED_BASE,
    ])

    return characteristics_base.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=service_uuid,
        optional_service_uuid=optional_service_uuid,
        characteristic_uuid=characteristic_uuid,
        optional_characteristic_uuid=optional_characteristic_uuid)


def get_get_primary_services_call():
    call = random.choice([
        u'getPrimaryService({service_uuid})',
        u'getPrimaryServices({optional_service_uuid})'
    ])

    return call.format(
        service_uuid=get_service_uuid(),
        optional_service_uuid=random.choice(['', get_service_uuid()]))


def get_characteristics_call():
    call = random.choice([
        u'getCharacteristic({characteristic_uuid})',
        u'getCharacteristics({optional_characteristic_uuid})'
    ])

    return call.format(
        characteristic_uuid=get_characteristic_uuid(),
        optional_characteristic_uuid=random.choice(
            ['', get_characteristic_uuid()]))


def get_pick_a_service():
    """Returns a string that picks a service from 'services'."""
    # 'services' may be defined by the GetPrimaryService(s) tokens.
    string = \
        'var service; '\
        'if (typeof services !== \'undefined\') '\
        ' service = Array.isArray(services)'\
        ' ? services[{} % services.length]'\
        ' : services'
    return string.format(random.randint(0, sys.maxsize))


def get_pick_a_characteristic():
    """Returns a string that picks a characteristic from 'characteristics'."""
    # 'characteristics' maybe be defined by the GetCharacteristic(s) tokens.
    string = \
        'var characteristic; '\
        'if (typeof characteristics !== \'undefined\') '\
        ' characteristic = Array.isArray(characteristics)'\
        ' ? characteristics[{} % characteristics.length]'\
        ' : characteristics'
    return string.format(random.randint(0, sys.maxsize))


def get_reload_id():
    return _ToJsStr(_get_random_number())


def get_buffer_source():
    """Returns a new BufferSource.
    https://webidl.spec.whatwg.org/#BufferSource
    """

    choice = random.choice(['ArrayBuffer', 'DataView', 'TypedArray'])

    if choice == 'ArrayBuffer':
        # We choose 2**10 as the upper boundry because the max length allowed
        # by WebBluetooth is 2**10.
        return 'new ArrayBuffer({length})'.format(
            length=utils.UniformExpoInteger(0, 10))
    if choice == 'DataView':
        return 'new DataView({typed_array}.buffer)'.format(
            typed_array=_get_typed_array())
    if choice == 'TypedArray':
        return _get_typed_array()
