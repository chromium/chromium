#!/usr/bin/env python

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple script used to generate the GNU hash table test data"""

import collections
import os

from pylib import source_utils
from pylib import elf_utils

script_name = os.path.basename(__file__)


def GnuHash(name):
  """Compute the GNU hash of a given input string."""
  h = 5381
  for c in name:
    h = (h * 33 + ord(c)) & 0xffffffff
  return h


class BloomFilter(object):
  """A class used to model the bloom filter used in GNU hash tables.

  Usage is the following:
    1) Create new instance.
    2) Call Add() repeatedly to add new entries for each symbol hash value.
    3) Call AsCSourceUint32Array() to generate a C source fragment
       corresponding to the content of an array of 32-bit words for the
       filter.
    4) Also use __str__() to print a human-friendly representation of the
       filter to check everything if needed.
  """
  def __init__(self, bloom_size, bloom_shift, bloom_bits):
    """Create instance.

    Args:
      bloom_size: number of words in the bloom filter.
      bloom_shift: bloom bit shift to use for secondary bit.
      bloom_bits: number of bits in each filter word (32 or 64).
    """
    self.bloom_size_ = bloom_size
    self.bloom_shift_ = bloom_shift
    self.bloom_bits_ = bloom_bits
    self.bloom_filter_ = [0] * bloom_size

  def GetBits(self, gnu_hash):
    """Return (index, bit0, bit1) tuple corresponding to a given hash."""
    bloom_index = (gnu_hash / self.bloom_bits_) % self.bloom_size_
    bloom_bit0 = (gnu_hash % self.bloom_bits_)
    bloom_bit1 = ((gnu_hash >> self.bloom_shift_) % self.bloom_bits_)
    return bloom_index, bloom_bit0, bloom_bit1

  def Add(self, gnu_hash):
    """Add a new entry to the filter."""
    word, bit0, bit1 = self.GetBits(gnu_hash)
    self.bloom_filter_[word] |= (1 << bit0) | (1 << bit1)

  def AsCSourceUint32Array(self):
    """Generate C source fragment for 32-bit uint array data."""
    if self.bloom_bits_ == 64:
      # Convert to array of 32-bit values first. Assume little-endianess.
      values = []
      for bloom in self.bloom_filter_:
        values += [bloom & 0xffffffff, (bloom >> 32) % 0xffffffff]
    else:
      values = self.bloom_filter_
    return source_utils.CSourceForIntegerHexArray(values, 32)

  def __str__(self):
    """Convert bloom filter instance to human-friendly representation."""
    out = 'Bloom filter (%d bits):\n' % self.bloom_bits_
    out += 'bit#'
    if self.bloom_bits_ == 64:
      out += '       56       48       40       32'
    out += '       24       16        8        0'
    for bloom in self.bloom_filter_:
      for n in range(self.bloom_bits_):
        if (n % 8) == 0:
          if n > 0:
            out += ' '
          else:
            out += '\n     '

        if ((bloom & (1 << (self.bloom_bits_ - n - 1))) != 0):
          out += 'x'
        else:
          out += '.'
    out += '\n\n  also as: '
    for bloom in self.bloom_filter_:
      if self.bloom_bits_ == 64:
        out += ' 0x%016x' % bloom
      else:
        out += ' 0x%08x' % bloom
    out += '\n'
    return out


class GnuHashTable(object):
  def __init__(self, sym_offset, num_buckets, bloom_size, bloom_shift,
               bloom_bits, symbol_names):
    """Initialize a new GNU hash table instance.

    Args:
      sym_offset: Dynamic symbols offset, must be > 0.
      num_buckets: Number of hash buckets, must be > 0.
      bloom_size: Bloom filter size in words of |bloom_bits| bits.
      bloom_shift: Bloom filter shift.
      bloom_bits: Either 32 or 64, size of bloom filter words.
      symbol_names: List of symbol names.
    """
    self.num_buckets_ = num_buckets
    self.sym_offset_ = sym_offset
    self.bloom_size_ = bloom_size
    self.bloom_shift_ = bloom_shift

    # Create a list of (symbol, hash) values, sorted by increasing bucket index
    sorted_symbols = sorted([(x, GnuHash(x)) for x in symbol_names],
                            key=lambda t: t[1] % num_buckets)

    self.symbols_ = [t[0] for t in sorted_symbols]
    self.hashes_ = [t[1] for t in sorted_symbols]

    # Build bucket and chain arrays.
    buckets = [0] * num_buckets
    chain = [0] * len(sorted_symbols)

    last_bucket_index = -1
    for n, symbol in enumerate(sorted_symbols):
      gnu_hash = self.hashes_[n]
      bucket_index = gnu_hash % num_buckets
      if bucket_index != last_bucket_index:
        buckets[bucket_index] = n + sym_offset
        last_bucket_index = bucket_index
        if n > 0: chain[n - 1] |= 1
      chain[n] = gnu_hash & ~1

    if chain: chain[-1] |= 1

    self.buckets_ = buckets
    self.chain_ = chain

    # Generate bloom filters for both 32 and 64 bits.
    self.bloom_filter32_ = self._GenerateBloomFilter(32)
    self.bloom_filter64_ = self._GenerateBloomFilter(64)

    # Generate final string table and symbol offsets.
    self.string_table_, self.symbol_offsets_ = \
        elf_utils.GenerateStringTable(self.symbols_)

  def _GenerateBloomFilter(self, bloom_bits):
    """Generate bloom filter array for a specific bitness."""
    bloom = BloomFilter(self.bloom_size_, self.bloom_shift_, bloom_bits)
    for gnu_hash in self.hashes_:
      bloom.Add(gnu_hash)
    return bloom

  def __str__(self):
    """Human friendly text description for this table."""
    out = 'GNU hash table: num_buckets=%d bloom_size=%d bloom_shift=%d\n\n' % (
        self.num_buckets_, self.bloom_size_, self.bloom_shift_)

    out += 'idx symbol               hash      bucket  bloom32  bloom64  chain\n\n'
    out += '  0 ST_UNDEF\n'
    for n, symbol in enumerate(self.symbols_):
      gnu_hash = self.hashes_[n]
      bucket_index = gnu_hash % self.num_buckets_
      bloom32_index, bloom32_bit0, bloom32_bit1 = self.bloom_filter32_.GetBits(gnu_hash)
      bloom64_index, bloom64_bit0, bloom64_bit1 = self.bloom_filter64_.GetBits(gnu_hash)
      out += '%3d %-20s %08x  %-3d     %d:%02d:%02d  %d:%02d:%02d  %08x\n' % (
          n + 1, symbol, gnu_hash, bucket_index, bloom32_index, bloom32_bit0,
          bloom32_bit1, bloom64_index, bloom64_bit0, bloom64_bit1,
          self.chain_[n])

    out += '\nBuckets: '
    comma = ''
    for b in self.buckets_:
      out += '%s%d' % (comma, b)
      comma = ', '

    out += '\n\n%s\n%s' % (self.bloom_filter32_, self.bloom_filter64_)
    return out

  def AsCSource(self, variable_prefix, guard_macro_name):
    """Dump the content of this instance."""
    out = source_utils.CSourceBeginAutoGeneratedHeader(script_name,
                                                       guard_macro_name)

    out += source_utils.CSourceForComments(str(self))

    out += source_utils.CSourceForConstCharArray(
        self.string_table_, 'k%sStringTable' % variable_prefix)

    out += '\n'
    out += elf_utils.CSourceForElfSymbolListMacro(variable_prefix,
                                                  self.symbols_,
                                                  self.symbol_offsets_)
    out += '\n'
    out += elf_utils.CSourceForElfSymbolTable(variable_prefix,
                                              self.symbols_,
                                              self.symbol_offsets_)

    out += '\nstatic const uint32_t k%sHashTable[] = {\n' % variable_prefix
    out += '    %d,  // num_buckets\n' % self.num_buckets_
    out += '    %d,  // sym_offset\n' % self.sym_offset_
    out += '    %d,  // bloom_size\n' % self.bloom_size_
    out += '    %d,  // bloom_shift\n' % self.bloom_shift_
    out += '    // Bloom filter words\n'
    out += '#if UINTPTR_MAX == UINT32_MAX  // 32-bit bloom filter words\n'
    out += self.bloom_filter32_.AsCSourceUint32Array()
    out += '#else  // 64-bits filter bloom words (assumes little-endianess)\n'
    out += self.bloom_filter64_.AsCSourceUint32Array()
    out += '#endif  // bloom filter words\n'
    out += '    // Buckets\n'
    out += source_utils.CSourceForIntegerHexArray(self.buckets_, 32)
    out += '    // Chain\n'
    out += source_utils.CSourceForIntegerHexArray(self.chain_, 32)
    out += '};\n'
    out += source_utils.CSourceEndAutoGeneratedHeader(script_name,
                                                      guard_macro_name)
    return out

def main():
  # Same data as the one found on the following web page, to ease verification:
  #
  #   https://flapenguin.me/2017/05/10/elf-lookup-dt-gnu-hash/
  #
  # NOTE: The bloom filter values and bitmaps on that page are widely incorrect
  #       but the bloom word and bit indices are ok though!
  #
  table = GnuHashTable(1, 4, 2, 5, 64, [
      'cfsetispeed', 'strsigna', 'hcreate_', 'endrpcen', 'uselib',
      'gettyen', 'umoun', 'freelocal', 'listxatt', 'isnan', 'isinf',
      'setrlimi', 'getspen', 'pthread_mutex_lock', 'getopt_long_onl',
      ])
  print table.AsCSource('TestGnu', 'CRAZY_LINKER_GNU_HASH_TABLE_TEST_DATA_H')

if __name__ == "__main__":
  main()
