# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import functools
import math
import random


def RandomLowInteger(low, high, beta=31.0):
  """Like random.randint, but heavily skewed toward the low end"""
  assert low <= high
  return low + int(math.floor(random.betavariate(1.0, beta) * (high - low)))


def UniformExpoInteger(low, high, base=2):
  """Returns base to a power uniformly distributed between low and high.

  This is useful for exploring large ranges of integers while ensuring that
  values of all different sizes are represented.
  """
  return int(math.floor(math.pow(base, random.uniform(low, high))))


def WeightedChoice(choices):  # pylint: disable=inconsistent-return-statements
  """Chooses an item given a sequence of (choice, weight) tuples"""
  total = sum(w for c, w in choices)
  r = random.uniform(0, total)
  upto = 0
  for c, w in choices:
    upto += w
    if upto >= r:
      return c
  assert False


def Pipeline(*funcs):
  """Given a number of single-argument functions, returns a single-argument
  function which computes their composition. Each of the functions are applied
  to the input in order from left to right, with the result of each function
  passed as the argument to the next function."""
  return functools.reduce(lambda f, g: lambda x: g(f(x)), funcs)


def DeepMemoize(obj):
  """A memoizing decorator that returns deep copies of the function results."""
  cache = obj.cache = {}

  @functools.wraps(obj)
  def Memoize(*args):
    if args not in cache:
      cache[args] = copy.deepcopy(obj(*args))
    return copy.deepcopy(cache[args])

  return Memoize
