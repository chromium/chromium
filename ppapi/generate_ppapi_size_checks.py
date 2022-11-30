#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script should be run manually on occasion to make sure all PPAPI types
have appropriate size checking.
"""

from __future__ import print_function

import optparse
import os
import subprocess
import sys


# The string that the PrintNamesAndSizes plugin uses to indicate a type is
# expected to have architecture-dependent size.
ARCH_DEPENDENT_STRING = "ArchDependentSize"


COPYRIGHT_STRING_C = (
"""/* Copyright %s The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file has compile assertions for the sizes of types that are dependent
 * on the architecture for which they are compiled (i.e., 32-bit vs 64-bit).
 */

""") % datetime.date.today().year


class SourceLocation(object):
  """A class representing the source location of a definiton."""

  def __init__(self, filename="", start_line=-1, end_line=-1):
    self.filename = os.path.normpath(filename)
    self.start_line = start_line
    self.end_line = end_line


class TypeInfo(object):
  """A class representing information about a C++ type.  It contains the
  following fields:
   - kind:  The Clang TypeClassName (Record, Enum, Typedef, Union, etc)
   - name:  The unmangled string name of the type.
   - size:  The size in bytes of the type.
   - arch_dependent:  True if the type may have architecture dependent size
                      according to PrintNamesAndSizes.  False otherwise.  Types
                      which are considered architecture-dependent from 32-bit
                      to 64-bit are pointers, longs, unsigned longs, and any
                      type that contains an architecture-dependent type.
   - source_location:  A SourceLocation describing where the type is defined.
   - target:  The target Clang was compiling when it found the type definition.
              This is used only for diagnostic output.
   - parsed_line:  The line which Clang output which was used to create this
                   TypeInfo (as the info_string parameter to __init__).  This is
                   used only for diagnostic output.
  """

  def __init__(self, info_string, target):
    """Create a TypeInfo from a given info_string.  Also store the name of the
    target for which the TypeInfo was first created just so we can print useful
    error information.
    info_string is a comma-delimited string of the following form:
    kind,name,size,arch_dependent,source_file,start_line,end_line
    Where:
   - kind:  The Clang TypeClassName (Record, Enum, Typedef, Union, etc)
   - name:  The unmangled string name of the type.
   - size:  The size in bytes of the type.
   - arch_dependent:  'ArchDependentSize' if the type has architecture-dependent
                      size, NotArchDependentSize otherwise.
   - source_file:  The source file in which the type is defined.
   - first_line:  The first line of the definition (counting from 0).
   - last_line:  The last line of the definition (counting from 0).
   This should match the output of the PrintNamesAndSizes plugin.
   """
    [self.kind, self.name, self.size, arch_dependent_string, source_file,
        start_line, end_line] = info_string.split(',')
    self.target = target
    self.parsed_line = info_string
    # Note that Clang counts line numbers from 1, but we want to count from 0.
    self.source_location = SourceLocation(source_file,
                                          int(start_line)-1,
                                          int(end_line)-1)
    self.arch_dependent = (arch_dependent_string == ARCH_DEPENDENT_STRING)


class FilePatch(object):
  """A class representing a set of line-by-line changes to a particular file.
  None of the changes are applied until Apply is called.  All line numbers are
  counted from 0.
  """

  def __init__(self, filename):
    self.filename = filename
    self.linenums_to_delete = set()
    # A dictionary from line number to an array of strings to be inserted at
    # that line number.
    self.lines_to_add = {}

  def Delete(self, start_line, end_line):
    """Make the patch delete the lines starting with |start_line| up to but not
    including |end_line|.
    """
    self.linenums_to_delete |= set(range(start_line, end_line))

  def Add(self, text, line_number):
    """Add the given text before the text on the given line number."""
    if line_number in self.lines_to_add:
      self.lines_to_add[line_number].append(text)
    else:
      self.lines_to_add[line_number] = [text]

  def Apply(self):
    """Apply the patch by writing it to self.filename."""
    # Read the lines of the existing file in to a list.
    sourcefile = open(self.filename, "r")
    file_lines = sourcefile.readlines()
    sourcefile.close()
    # Now apply the patch.  Our strategy is to keep the array at the same size,
    # and just edit strings in the file_lines list as necessary.  When we delete
    # lines, we just blank the line and keep it in the list.  When we add lines,
    # we just prepend the added source code to the start of the existing line at
    # that line number.  This way, all the line numbers we cached from calls to
    # Add and Delete remain valid list indices, and we don't have to worry about
    # maintaining any offsets.  Each element of file_lines at the end may
    # contain any number of lines (0 or more) delimited by carriage returns.
    for linenum_to_delete in self.linenums_to_delete:
      file_lines[linenum_to_delete] = "";
    for linenum, sourcelines in self.lines_to_add.items():
      # Sort the lines we're adding so we get relatively consistent results.
      sourcelines.sort()
      # Prepend the new lines.  When we output
      file_lines[linenum] = "".join(sourcelines) + file_lines[linenum]
    newsource = open(self.filename, "w")
    for line in file_lines:
      newsource.write(line)
    newsource.close()


def CheckAndInsert(typeinfo, typeinfo_map):
  """Check if a TypeInfo exists already in the given map with the same name.  If
  so, make sure the size is consistent.
  - If the name exists but the sizes do not match, print a message and
    exit with non-zero exit code.
  - If the name exists and the sizes match, do nothing.
  - If the name does not exist, insert the typeinfo in to the map.

  """
  # If the type is unnamed, ignore it.
  if typeinfo.name == "":
    return
  # If the size is 0, ignore it.
  elif int(typeinfo.size) == 0:
    return
  # If the type is not defined under ppapi, ignore it.
  elif typeinfo.source_location.filename.find("ppapi") == -1:
    return
  # If the type is defined under GLES2, ignore it.
  elif typeinfo.source_location.filename.find("GLES2") > -1:
    return
  # If the type is an interface (by convention, starts with PPP_ or PPB_),
  # ignore it.
  elif (typeinfo.name[:4] == "PPP_") or (typeinfo.name[:4] == "PPB_"):
    return
  elif typeinfo.name in typeinfo_map:
    if typeinfo.size != typeinfo_map[typeinfo.name].size:
      print("Error: '" + typeinfo.name + "' is", \
          typeinfo_map[typeinfo.name].size, \
          "bytes on target '" + typeinfo_map[typeinfo.name].target + \
          "', but", typeinfo.size, "on target '" + typeinfo.target + "'")
      print(typeinfo_map[typeinfo.name].parsed_line)
      print(typeinfo.parsed_line)
      sys.exit(1)
    else:
      # It's already in the map and the sizes match.
      pass
  else:
    typeinfo_map[typeinfo.name] = typeinfo


def ProcessTarget(clang_command, target, types):
  """Run clang using the given clang_command for the given target string.  Parse
  the output to create TypeInfos for each discovered type.  Insert each type in
  to the 'types' dictionary.  If the type already exists in the types
  dictionary, make sure that the size matches what's already in the map.  If
  not, exit with an error message.
  """
  p = subprocess.Popen(clang_command + " -triple " + target,
                       shell=True,
                       stdout=subprocess.PIPE)
  lines = p.communicate()[0].split()
  for line in lines:
    typeinfo = TypeInfo(line, target)
    CheckAndInsert(typeinfo, types)


def ToAssertionCode(typeinfo):
  """Convert the TypeInfo to an appropriate C compile assertion.
  If it's a struct (Record in Clang terminology), we want a line like this:
    PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(<name>, <size>);\n
  Enums:
    PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(<name>, <size>);\n
  Typedefs:
    PP_COMPILE_ASSERT_SIZE_IN_BYTES(<name>, <size>);\n

  """
  line = "PP_COMPILE_ASSERT_"
  if typeinfo.kind == "Enum":
    line += "ENUM_"
  elif typeinfo.kind == "Record":
    line += "STRUCT_"
  line += "SIZE_IN_BYTES("
  line += typeinfo.name
  line += ", "
  line += typeinfo.size
  line += ");\n"
  return line


def IsMacroDefinedName(typename):
  """Return true iff the given typename came from a PPAPI compile assertion."""
  return typename.find("PP_Dummy_Struct_For_") == 0


def WriteArchSpecificCode(types, root, filename):
  """Write a header file that contains a compile-time assertion for the size of
     each of the given typeinfos, in to a file named filename rooted at root.
  """
  assertion_lines = [ToAssertionCode(typeinfo) for typeinfo in types]
  assertion_lines.sort()
  outfile = open(os.path.join(root, filename), "w")
  header_guard = "PPAPI_TESTS_" + filename.upper().replace(".", "_") + "_"
  outfile.write(COPYRIGHT_STRING_C)
  outfile.write('#ifndef ' + header_guard + '\n')
  outfile.write('#define ' + header_guard + '\n\n')
  outfile.write('#include "ppapi/tests/test_struct_sizes.c"\n\n')
  for line in assertion_lines:
    outfile.write(line)
  outfile.write('\n#endif  /* ' + header_guard + ' */\n')


def main(argv):
  # See README file for example command-line invocation.  This script runs the
  # PrintNamesAndSizes Clang plugin with 'test_struct_sizes.c' as input, which
  # should include all C headers and all existing size checks.  It runs the
  # plugin multiple times;  once for each of a set of targets, some 32-bit and
  # some 64-bit.  It verifies that wherever possible, types have a consistent
  # size on both platforms.  Types that can't easily have consistent size (e.g.
  # ones that contain a pointer) are checked to make sure they are consistent
  # for all 32-bit platforms and consistent on all 64-bit platforms, but the
  # sizes on 32 vs 64 are allowed to differ.
  #
  # Then, if all the types have consistent size as expected, compile assertions
  # are added to the source code.  Types whose size is independent of
  # architectureacross have their compile assertions placed immediately after
  # their definition in the C API header.  Types whose size differs on 32-bit
  # vs 64-bit have a compile assertion placed in each of:
  # ppapi/tests/arch_dependent_sizes_32.h and
  # ppapi/tests/arch_dependent_sizes_64.h.
  #
  # Note that you should always check the results of the tool to make sure
  # they are sane.
  parser = optparse.OptionParser()
  parser.add_option(
      '-c', '--clang-path', dest='clang_path',
      default=(''),
      help='the path to the clang binary (default is to get it from your path)')
  parser.add_option(
      '-p', '--plugin', dest='plugin',
      default='tests/clang/libPrintNamesAndSizes.so',
      help='The path to the PrintNamesAndSizes plugin library.')
  parser.add_option(
      '--targets32', dest='targets32',
      default='i386-pc-linux,arm-pc-linux,i386-pc-win32',
      help='Which 32-bit target triples to provide to clang.')
  parser.add_option(
      '--targets64', dest='targets64',
      default='x86_64-pc-linux,x86_64-pc-win',
      help='Which 32-bit target triples to provide to clang.')
  parser.add_option(
      '-r', '--ppapi-root', dest='ppapi_root',
      default='.',
      help='The root directory of ppapi.')
  options, args = parser.parse_args(argv)
  if args:
    parser.print_help()
    print('ERROR: invalid argument')
    sys.exit(1)

  clang_executable = os.path.join(options.clang_path, 'clang')
  clang_command = clang_executable + " -cc1" \
      + " -load " + options.plugin \
      + " -plugin PrintNamesAndSizes" \
      + " -I" + os.path.join(options.ppapi_root, "..") \
      + " " \
      + os.path.join(options.ppapi_root, "tests", "test_struct_sizes.c")

  # Dictionaries mapping type names to TypeInfo objects.
  # Types that have size dependent on architecture, for 32-bit
  types32 = {}
  # Types that have size dependent on architecture, for 64-bit
  types64 = {}
  # Note that types32 and types64 should contain the same types, but with
  # different sizes.

  # Types whose size should be consistent regardless of architecture.
  types_independent = {}

  # Now run clang for each target.  Along the way, make sure architecture-
  # dependent types are consistent sizes on all 32-bit platforms and consistent
  # on all 64-bit platforms.
  targets32 = options.targets32.split(',');
  for target in targets32:
    # For each 32-bit target, run the PrintNamesAndSizes Clang plugin to get
    # information about all types in the translation unit, and add a TypeInfo
    # for each of them to types32.  If any size mismatches are found,
    # ProcessTarget will spit out an error and exit.
    ProcessTarget(clang_command, target, types32)
  targets64 = options.targets64.split(',');
  for target in targets64:
    # Do the same as above for each 64-bit target;  put all types in types64.
    ProcessTarget(clang_command, target, types64)

  # Now for each dictionary, find types whose size are consistent regardless of
  # architecture, and move those in to types_independent.  Anywhere sizes
  # differ, make sure they are expected to be architecture-dependent based on
  # their structure.  If we find types which could easily be consistent but
  # aren't, spit out an error and exit.
  types_independent = {}
  for typename, typeinfo32 in types32.items():
    if (typename in types64):
      typeinfo64 = types64[typename]
      if (typeinfo64.size == typeinfo32.size):
        # The types are the same size, so we can treat it as arch-independent.
        types_independent[typename] = typeinfo32
        del types32[typename]
        del types64[typename]
      elif (typeinfo32.arch_dependent or typeinfo64.arch_dependent):
        # The type is defined in such a way that it would be difficult to make
        # its size consistent.  E.g., it has pointers.  We'll leave it in the
        # arch-dependent maps so that we can put arch-dependent size checks in
        # test code.
        pass
      else:
        # The sizes don't match, but there's no reason they couldn't.  It's
        # probably due to an alignment mismatch between Win32/NaCl vs Linux32/
        # Mac32.
        print("Error: '" + typename + "' is", typeinfo32.size, \
            "bytes on target '" + typeinfo32.target + \
            "', but", typeinfo64.size, "on target '" + typeinfo64.target + "'")
        print(typeinfo32.parsed_line)
        print(typeinfo64.parsed_line)
        sys.exit(1)
    else:
      print("WARNING:  Type '", typename, "' was defined for target '")
      print(typeinfo32.target, ", but not for any 64-bit targets.")

  # Now we have all the information we need to generate our static assertions.
  # Types that have consistent size across architectures will have the static
  # assertion placed immediately after their definition.  Types whose size
  # depends on 32-bit vs 64-bit architecture will have checks placed in
  # tests/arch_dependent_sizes_32/64.h.

  # This dictionary maps file names to FilePatch objects.  We will add items
  # to it as needed.  Each FilePatch represents a set of changes to make to the
  # associated file (additions and deletions).
  file_patches = {}

  # Find locations of existing macros, and just delete them all.  Note that
  # normally, only things in 'types_independent' need to be deleted, as arch-
  # dependent checks exist in tests/arch_dependent_sizes_32/64.h, which are
  # always completely over-ridden.  However, it's possible that a type that used
  # to be arch-independent has changed to now be arch-dependent (e.g., because
  # a pointer was added), and we want to delete the old check in that case.
  for name, typeinfo in \
      types_independent.items() + types32.items() + types64.items():
    if IsMacroDefinedName(name):
      sourcefile = typeinfo.source_location.filename
      if sourcefile not in file_patches:
        file_patches[sourcefile] = FilePatch(sourcefile)
      file_patches[sourcefile].Delete(typeinfo.source_location.start_line,
                                      typeinfo.source_location.end_line+1)

  # Add a compile-time assertion for each type whose size is independent of
  # architecture.  These assertions go immediately after the class definition.
  for name, typeinfo in types_independent.items():
    # Ignore dummy types that were defined by macros and also ignore types that
    # are 0 bytes (i.e., typedefs to void).
    if not IsMacroDefinedName(name) and typeinfo.size > 0:
      sourcefile = typeinfo.source_location.filename
      if sourcefile not in file_patches:
        file_patches[sourcefile] = FilePatch(sourcefile)
      # Add the assertion code just after the definition of the type.
      # E.g.:
      # struct Foo {
      #   int32_t x;
      # };
      # PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(Foo, 4); <---Add this line
      file_patches[sourcefile].Add(ToAssertionCode(typeinfo),
                                   typeinfo.source_location.end_line+1)

  # Apply our patches.  This actually edits the files containing the definitions
  # for the types in types_independent.
  for filename, patch in file_patches.items():
    patch.Apply()

  # Write out a file of checks for 32-bit architectures and a separate file for
  # 64-bit architectures.  These only have checks for types that are
  # architecture-dependent.
  c_source_root = os.path.join(options.ppapi_root, "tests")
  WriteArchSpecificCode(types32.values(),
                        c_source_root,
                        "arch_dependent_sizes_32.h")
  WriteArchSpecificCode(types64.values(),
                        c_source_root,
                        "arch_dependent_sizes_64.h")

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
