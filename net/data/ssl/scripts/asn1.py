# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file implements very minimal ASN.1, DER serialization.


def ToDER(obj):
  '''ToDER converts the given object into DER encoding'''
  if obj is None:
    # None turns into NULL
    return TagAndLength(5, 0)
  if isinstance(obj, (str, bytes)):
    # There are many ASN.1 string types, so rather than pick one implicitly,
    # require the caller explicitly specify the encoding with asn1.UTF8String,
    # etc., below.
    raise TypeError("String types must be specified explicitly")
  if isinstance(obj, bool):
    val = b"\x00"
    if obj:
      val = b"\xff"
    return TagAndData(1, val)
  if isinstance(obj, int):
    big_endian = bytearray()
    val = obj
    while val != 0:
      big_endian.append(val & 0xff)
      val >>= 8

    if len(big_endian) == 0 or big_endian[-1] >= 128:
      big_endian.append(0)

    big_endian.reverse()
    return TagAndData(2, bytes(big_endian))

  return obj.ToDER()


def TagAndLength(tag, length):
  der = bytearray([tag])
  if length < 128:
    der.append(length)
  elif length < 256:
    der.append(0x81)
    der.append(length)
  elif length < 65535:
    der.append(0x82)
    der.append(length >> 8)
    der.append(length & 0xff)
  else:
    assert False

  return bytes(der)


def TagAndData(tag, data):
  return TagAndLength(tag, len(data)) + data


class Raw(object):
  '''Raw contains raw DER encoded bytes that are used verbatim'''

  def __init__(self, der):
    self.der = der

  def ToDER(self):
    return self.der


class Explicit(object):
  '''Explicit prepends an explicit tag'''

  def __init__(self, tag, child):
    self.tag = tag
    self.child = child

  def ToDER(self):
    der = ToDER(self.child)
    tag = self.tag
    tag |= 0x80  # content specific
    tag |= 0x20  # complex
    return TagAndData(tag, der)


class ENUMERATED(object):
  def __init__(self, value):
    self.value = value

  def ToDER(self):
    return TagAndData(10, bytes([self.value]))


class SEQUENCE(object):
  def __init__(self, children):
    self.children = children

  def ToDER(self):
    der = b''.join([ToDER(x) for x in self.children])
    return TagAndData(0x30, der)


class SET(object):
  def __init__(self, children):
    self.children = children

  def ToDER(self):
    der = b''.join([ToDER(x) for x in self.children])
    return TagAndData(0x31, der)


class OCTETSTRING(object):
  def __init__(self, val):
    self.val = val

  def ToDER(self):
    return TagAndData(4, self.val)


class PrintableString(object):
  def __init__(self, val):
    self.val = val

  def ToDER(self):
    return TagAndData(19, self.val)


class UTF8String(object):
  def __init__(self, val):
    self.val = val

  def ToDER(self):
    return TagAndData(12, self.val)


class OID(object):
  def __init__(self, parts):
    self.parts = parts

  def ToDER(self):
    if len(self.parts) < 2 or self.parts[0] > 6 or self.parts[1] >= 40:
      assert False

    der = bytearray([self.parts[0] * 40 + self.parts[1]])
    for x in self.parts[2:]:
      if x == 0:
        der.append(0)
      else:
        octets = bytearray()
        while x != 0:
          v = x & 0x7f
          if len(octets) > 0:
            v |= 0x80
          octets.append(v)
          x >>= 7
        octets.reverse()
        der = der + octets

    return TagAndData(6, bytes(der))


class UTCTime(object):
  def __init__(self, time_str):
    self.time_str = time_str

  def ToDER(self):
    return TagAndData(23, self.time_str.encode('ascii'))


class GeneralizedTime(object):
  def __init__(self, time_str):
    self.time_str = time_str

  def ToDER(self):
    return TagAndData(24, self.time_str.encode('ascii'))


class BitString(object):
  def __init__(self, bits):
    self.bits = bits

  def ToDER(self):
    return TagAndData(3, b"\x00" + self.bits)
