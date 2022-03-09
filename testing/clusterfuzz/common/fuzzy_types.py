# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import functools
import itertools
import os
import random
import re
import string
import sys
import textwrap

from . import utils
if sys.version_info.major == 3:
  unichr = chr    # pylint: disable=redefined-builtin


def FuzzyInt(n):
  """Returns an integer derived from the input by one of several mutations."""
  int_sizes = [8, 16, 32, 64, 128]
  mutations = [
    lambda n: utils.UniformExpoInteger(0, sys.maxsize.bit_length() + 1),
    lambda n: -utils.UniformExpoInteger(0, sys.maxsize.bit_length()),
    lambda n: 2 ** random.choice(int_sizes) - 1,
    lambda n: 2 ** random.choice(int_sizes),
    lambda n: 0,
    lambda n: -n,
    lambda n: n + 1,
    lambda n: n - 1,
    lambda n: n + random.randint(-1024, 1024),
  ]
  return random.choice(mutations)(n)


def FuzzyString(s):
  """Returns a string derived from the input by one of several mutations."""
  # First try some mutations that try to recognize certain types of strings
  try:
    s.decode("utf-8")  # These mutations only make sense for textual data
  except UnicodeDecodeError:
    pass
  else:
    chained_mutations = [
      FuzzIntsInString,
      FuzzBase64InString,
      FuzzListInString,
    ]
    original = s
    for mutation in chained_mutations:
      s = mutation(s)
      # Stop if we've modified the string and our coin comes up heads
      if s != original and random.getrandbits(1):
        return s

  # If we're still here, apply a more generic mutation
  mutations = [
    lambda s: "".join(random.choice(string.printable) for i in
      range(utils.UniformExpoInteger(0, 14))),
    lambda s: "".join(unichr(random.randint(0, sys.maxunicode)) for i in
      range(utils.UniformExpoInteger(0, 14))).encode("utf-8"),
    lambda s: os.urandom(utils.UniformExpoInteger(0, 14)),
    lambda s: s * utils.UniformExpoInteger(1, 5),
    lambda s: s + "A" * utils.UniformExpoInteger(0, 14),
    lambda s: "A" * utils.UniformExpoInteger(0, 14) + s,
    lambda s: s[:-random.randint(1, max(1, len(s) - 1))],
    lambda s: textwrap.fill(s, random.randint(1, max(1, len(s) - 1))),
    lambda s: "",
  ]
  return random.choice(mutations)(s)


def FuzzIntsInString(s):
  """Returns a string where some integers have been fuzzed with FuzzyInt."""
  def ReplaceInt(m):
    val = m.group()
    if random.getrandbits(1):  # Flip a coin to decide whether to fuzz
      return val
    if not random.getrandbits(4):  # Delete the integer 1/16th of the time
      return ""
    decimal = val.isdigit()  # Assume decimal digits means a decimal number
    n = FuzzyInt(int(val) if decimal else int(val, 16))
    return str(n) if decimal else "%x" % n
  return re.sub(r"\b[a-fA-F]*\d[0-9a-fA-F]*\b", ReplaceInt, s)


def FuzzBase64InString(s):
  """Returns a string where Base64 components are fuzzed with FuzzyBuffer."""
  def ReplaceBase64(m):
    fb = FuzzyBuffer(base64.b64decode(m.group()))
    fb.RandomMutation()
    return base64.b64encode(fb)
  # This only matches obvious Base64 words with trailing equals signs
  return re.sub(r"(?<![A-Za-z0-9+/])"
                r"(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)"
                r"(?![A-Za-z0-9+/])", ReplaceBase64, s)


def FuzzListInString(s, separators=r", |,|; |;|\r\n|\s"):
  """Tries to interpret the string as a list, and fuzzes it if successful."""
  seps = re.findall(separators, s)
  if not seps:
    return s
  sep = random.choice(seps)  # Ones that appear often are more likely
  items = FuzzyList(s.split(sep))
  items.RandomMutation()
  return sep.join(items)

# Pylint doesn't recognize that in this case 'self' is some mutable sequence,
# so the unsupoorted-assignment-operation and unsupported-delete-operation
# warnings have been disabled here.
# pylint: disable=unsupported-assignment-operation,unsupported-delete-operation
class FuzzySequence(object): #pylint: disable=useless-object-inheritance
  """A helpful mixin for writing fuzzy mutable sequence types.

  If a method parameter is left at its default value of None, an appropriate
  random value will be chosen.
  """

  def Overwrite(self, value, location=None, amount=None):
    """Overwrite amount elements starting at location with value.

    Value can be a function of no arguments, in which case it will be called
    every time a new value is needed.
    """
    if location is None:
      location = random.randint(0, max(0, len(self) - 1))
    if amount is None:
      amount = utils.RandomLowInteger(min(1, len(self)), len(self) - location)
    if hasattr(value, "__call__"):
      new_elements = (value() for i in range(amount))
    else:
      new_elements = itertools.repeat(value, amount)
    self[location:location+amount] = new_elements

  def Insert(self, value, location=None, amount=None, max_exponent=14):
    """Insert amount elements starting at location.

    Value can be a function of no arguments, in which case it will be called
    every time a new value is needed.
    """
    if location is None:
      location = random.randint(0, max(0, len(self) - 1))
    if amount is None:
      amount = utils.UniformExpoInteger(0, max_exponent)
    if hasattr(value, "__call__"):
      new_elements = (value() for i in range(amount))
    else:
      new_elements = itertools.repeat(value, amount)
    self[location:location] = new_elements

  def Delete(self, location=None, amount=None):
    """Delete amount elements starting at location."""
    if location is None:
      location = random.randint(0, max(0, len(self) - 1))
    if amount is None:
      amount = utils.RandomLowInteger(min(1, len(self)), len(self) - location)
    del self[location:location+amount]
# pylint: enable=unsupported-assignment-operation,unsupported-delete-operation


class FuzzyList(list, FuzzySequence):
  """A list with additional methods for fuzzing."""

  def RandomMutation(self, count=None, new_element=""):
    """Apply count random mutations chosen from a list."""
    random_items = lambda: random.choice(self) if self else new_element
    mutations = [
      lambda: random.shuffle(self),
      self.reverse,
      functools.partial(self.Overwrite, new_element),
      functools.partial(self.Overwrite, random_items),
      functools.partial(self.Insert, new_element, max_exponent=10),
      functools.partial(self.Insert, random_items, max_exponent=10),
      self.Delete,
    ]
    if count is None:
      count = utils.RandomLowInteger(1, 5, beta=3.0)
    for _ in range(count):
      random.choice(mutations)()


class FuzzyBuffer(bytearray, FuzzySequence):
  """A bytearray with additional methods for mutating the sequence of bytes."""

  def __repr__(self):
    return "%s(%r)" % (self.__class__.__name__, str(self))

  def FlipBits(self, num_bits=None):
    """Flip num_bits bits in the buffer at random."""
    if num_bits is None:
      num_bits = utils.RandomLowInteger(min(1, len(self)), len(self) * 8)
    for bit in random.sample(range(len(self) * 8), num_bits):
      self[bit / 8] ^= 1 << (bit % 8)

  def RandomMutation(self, count=None):
    """Apply count random mutations chosen from a weighted list."""
    random_bytes = lambda: random.randint(0x00, 0xFF)
    mutations = [
      (self.FlipBits, 1),
      (functools.partial(self.Overwrite, random_bytes), 1/3.0),
      (functools.partial(self.Overwrite, 0xFF), 1/3.0),
      (functools.partial(self.Overwrite, 0x00), 1/3.0),
      (functools.partial(self.Insert, random_bytes), 1/3.0),
      (functools.partial(self.Insert, 0xFF), 1/3.0),
      (functools.partial(self.Insert, 0x00), 1/3.0),
      (self.Delete, 1),
    ]
    if count is None:
      count = utils.RandomLowInteger(1, 5, beta=3.0)
    for _ in range(count):
      utils.WeightedChoice(mutations)()
