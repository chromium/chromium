# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script for extracting information from ELF files"""

import struct


class Error(Exception):
  '''Local Error class for this file.'''
  pass


def ParseElfHeader(path):
  """Determine properties of a nexe by parsing elf header.
  Return tuple of architecture and boolean signalling whether
  the executable is dynamic (has INTERP header) or static.
  """
  # From elf.h:
  # typedef struct
  # {
  #   unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  #   Elf64_Half e_type; /* Object file type */
  #   Elf64_Half e_machine; /* Architecture */
  #   ...
  # } Elf32_Ehdr;
  elf_header_format = '16s2H'
  elf_header_size = struct.calcsize(elf_header_format)

  with open(path, 'rb') as f:
    header = f.read(elf_header_size)

  try:
    header = struct.unpack(elf_header_format, header)
  except struct.error:
    raise Error("error parsing elf header: %s" % path)
  e_ident, _, e_machine = header[:3]

  elf_magic = b'\x7fELF'
  if e_ident[:4] != elf_magic:
    raise Error('Not a valid NaCl executable: %s' % path)

  e_machine_mapping = {
    3 : 'x86-32',
    8 : 'mips32',
    40 : 'arm',
    62 : 'x86-64'
  }
  if e_machine not in e_machine_mapping:
    raise Error('Unknown machine type: %s' % e_machine)

  # Set arch based on the machine type in the elf header
  arch = e_machine_mapping[e_machine]

  # Now read the full header in either 64bit or 32bit mode
  dynamic = IsDynamicElf(path, arch == 'x86-64')
  return arch, dynamic


def IsDynamicElf(path, is64bit):
  """Examine an elf file to determine if it is dynamically
  linked or not.
  This is determined by searching the program headers for
  a header of type PT_INTERP.
  """
  if is64bit:
    elf_header_format = '16s2HI3QI3H'
  else:
    elf_header_format = '16s2HI3II3H'

  elf_header_size = struct.calcsize(elf_header_format)

  with open(path, 'rb') as f:
    header = f.read(elf_header_size)
    header = struct.unpack(elf_header_format, header)
    p_header_offset = header[5]
    p_header_entry_size = header[9]
    num_p_header = header[10]
    f.seek(p_header_offset)
    p_headers = f.read(p_header_entry_size*num_p_header)

  # Read the first word of each Phdr to find out its type.
  #
  # typedef struct
  # {
  #   Elf32_Word  p_type;     /* Segment type */
  #   ...
  # } Elf32_Phdr;
  elf_phdr_format = 'I'
  PT_INTERP = 3

  while p_headers:
    p_header = p_headers[:p_header_entry_size]
    p_headers = p_headers[p_header_entry_size:]
    phdr_type = struct.unpack(elf_phdr_format, p_header[:4])[0]
    if phdr_type == PT_INTERP:
      return True

  return False
