# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file implements very minimal ASN.1, DER serialization.

import types


def ToDER(obj):
  '''ToDER converts the given object into DER encoding'''
  if type(obj) == types.NoneType:
    # None turns into NULL
    return TagAndLength(5, 0)
  if type(obj) == types.StringType:
    # Strings are PRINTABLESTRING
    return TagAndLength(19, len(obj)) + obj
  if type(obj) == types.UnicodeType:
    # Encode Unicode strings as UTF8String.
    utf8val = obj.encode('utf-8')
    return TagAndLength(12, len(utf8val)) + utf8val
  if type(obj) == types.BooleanType:
    val = "\x00"
    if obj:
      val = "\xff"
    return TagAndLength(1, 1) + val
  if type(obj) == types.IntType or type(obj) == types.LongType:
    big_endian = []
    val = obj
    while val != 0:
      big_endian.append(val & 0xff)
      val >>= 8

    if len(big_endian) == 0 or big_endian[-1] >= 128:
      big_endian.append(0)

    big_endian.reverse()
    return TagAndLength(2, len(big_endian)) + ToBytes(big_endian)

  return obj.ToDER()


def ToBytes(array_of_bytes):
  '''ToBytes converts the array of byte values into a binary string'''
  return ''.join([chr(x) for x in array_of_bytes])


def TagAndLength(tag, length):
  der = [tag]
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

  return ToBytes(der)


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
    tag |= 0x80 # content specific
    tag |= 0x20 # complex
    return TagAndLength(tag, len(der)) + der


class ENUMERATED(object):
  def __init__(self, value):
    self.value = value

  def ToDER(self):
    return TagAndLength(10, 1) + chr(self.value)


class SEQUENCE(object):
  def __init__(self, children):
    self.children = children

  def ToDER(self):
    der = ''.join([ToDER(x) for x in self.children])
    return TagAndLength(0x30, len(der)) + der


class SET(object):
  def __init__(self, children):
    self.children = children

  def ToDER(self):
    der = ''.join([ToDER(x) for x in self.children])
    return TagAndLength(0x31, len(der)) + der


class OCTETSTRING(object):
  def __init__(self, val):
    self.val = val

  def ToDER(self):
    return TagAndLength(4, len(self.val)) + self.val


class OID(object):
  def __init__(self, parts):
    self.parts = parts

  def ToDER(self):
    if len(self.parts) < 2 or self.parts[0] > 6 or self.parts[1] >= 40:
      assert False

    der = [self.parts[0]*40 + self.parts[1]]
    for x in self.parts[2:]:
      if x == 0:
        der.append(0)
      else:
        octets = []
        while x != 0:
          v = x & 0x7f
          if len(octets) > 0:
            v |= 0x80
          octets.append(v)
          x >>= 7
        octets.reverse()
        der = der + octets

    return TagAndLength(6, len(der)) + ToBytes(der)


class UTCTime(object):
  def __init__(self, time_str):
    self.time_str = time_str

  def ToDER(self):
    return TagAndLength(23, len(self.time_str)) + self.time_str


class GeneralizedTime(object):
  def __init__(self, time_str):
    self.time_str = time_str

  def ToDER(self):
    return TagAndLength(24, len(self.time_str)) + self.time_str


class BitString(object):
  def __init__(self, bits):
    self.bits = bits

  def ToDER(self):
    return TagAndLength(3, 1 + len(self.bits)) + "\x00" + self.bits
