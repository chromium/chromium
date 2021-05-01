import base64
import math
import struct

import six

from tests.support.asserts import assert_png


def decodebytes(s):
    return base64.decodebytes(six.ensure_binary(s))

def png_dimensions(screenshot):
    assert_png(screenshot)
    image = decodebytes(screenshot)
    width, height = struct.unpack(">LL", image[16:24])
    return int(width), int(height)
