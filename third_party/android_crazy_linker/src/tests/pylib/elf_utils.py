# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common ELF related routines."""

def GenerateStringTable(symbol_names):
  """Generate the string table that corresponds to a list of symbol names.

  Args:
    symbol_names: List of input symbol names.
  Returns:
    A (string_table, symbol_offsets) tuple, where |string_table| is the
    actual string table (terminated by two '\0' chars), and |symbol_offsets|
    is a list of starting offsets for each symbol inside the table.
  """
  symbol_offsets = []
  string_table = "\0"
  next_offset = 1
  for symbol in symbol_names:
    symbol_offsets.append(next_offset)
    string_table += symbol
    string_table += "\0"
    next_offset += len(symbol) + 1

  string_table += "\0"
  return string_table, symbol_offsets


def CSourceForElfSymbolTable(variable_prefix, names, str_offsets):
  """Generate C source definition for an ELF symbol table.

  Args:
    variable_prefix: variable name prefix
    names: List of symbol names.
    str_offsets: List of symbol name offsets in string table.
  Returns:
    String containing C source fragment.
  """
  out = (
r'''// NOTE: ELF32_Sym and ELF64_Sym have very different layout.
#if UINTPTR_MAX == UINT32_MAX  // ELF32_Sym
#  define DEFINE_ELF_SYMBOL(name, name_offset, address, size) \
    { (name_offset), (address), (size), ELF_ST_INFO(STB_GLOBAL, STT_FUNC), \
      0 /* other */, 1 /* shndx */ },
#else  // ELF64_Sym
#  define DEFINE_ELF_SYMBOL(name, name_offset, address, size) \
    { (name_offset), ELF_ST_INFO(STB_GLOBAL, STT_FUNC), \
      0 /* other */, 1 /* shndx */, (address), (size) },
#endif  // !ELF64_Sym
''')

  out += 'static const ELF::Sym k%sSymbolTable[] = {\n' % variable_prefix
  out += '    { 0 },  // ST_UNDEF\n'
  out += '    LIST_ELF_SYMBOLS_%s(DEFINE_ELF_SYMBOL)\n' % variable_prefix
  out += '};\n'
  out += '#undef DEFINE_ELF_SYMBOL\n'
  return out


def CSourceForElfSymbolListMacro(variable_prefix, names, name_offsets,
                                 base_address=0x10000, symbol_size=16,
                                 spacing_size=16):
  """Generate C source definition for a macro listing ELF symbols.

  Args:
    macro_suffix: Macro name suffix.
    names: List of symbol names.
    name_offsets: List of symbol offsets.
    base_address: Base starting address for symbols,
    symbol_size: Symbol size in bytes (all have the same size).
    spacing_size: Additionnal bytes between symbols.
  Returns:
    String containing C source fragment.
  """
  out = (
r'''// Auto-generated macro used to list all symbols
// XX must be a macro that takes the following parameters:
//   name: symbol name (quoted).
//   str_offset: symbol name offset in string table
//   address: virtual address.
//   size: size in bytes
''')
  out += '#define LIST_ELF_SYMBOLS_%s(XX) \\\n' % variable_prefix
  address = base_address
  for sym, offset in zip(names, name_offsets):
    out += '    XX("%s", %d, 0x%x, %d) \\\n' % (
        sym, offset, address, symbol_size)
    address += symbol_size + spacing_size
  out += '    // END OF LIST\n'
  return out
