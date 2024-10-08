# Copyright (C) 2010, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""GDB support for Blink types.

Add this to your gdb by amending your ~/.gdbinit as follows:
  python
  import sys
  sys.path.insert(0, "/path/to/blink/tools/gdb/")
  import blink
"""

from __future__ import print_function

import gdb
import re
import struct


def guess_string_length(ptr):
    """Guess length of string pointed by ptr.

    Returns a tuple of (length, an error message).
    """
    # Try to guess at the length.
    for i in range(0, 2048):
        try:
            if int((ptr + i).dereference()) == 0:
                return i, ''
        except RuntimeError:
            # We indexed into inaccessible memory; give up.
            return i, ' (gdb hit inaccessible memory)'
    return 256, ' (gdb found no trailing NUL)'


def ustring_to_string(ptr, length=None):
    """Convert a pointer to UTF-16 data into a Python string encoded with utf-8.

    ptr and length are both gdb.Value objects.
    If length is unspecified, will guess at the length."""
    error_message = ''
    if length is None:
        length, error_message = guess_string_length(ptr)
    else:
        length = int(length)
    char_vals = [int((ptr + i).dereference()) for i in range(length)]
    string = struct.pack('H' * length, *char_vals).decode(
        'utf-16', 'replace').encode('utf-8')
    return string + error_message.encode('utf-8')


def lstring_to_string(ptr, length=None):
    """Convert a pointer to LChar* data into a Python (non-Unicode) string.

    ptr and length are both gdb.Value objects.
    If length is unspecified, will guess at the length."""
    error_message = ''
    if length is None:
        length, error_message = guess_string_length(ptr)
    else:
        length = int(length)
    string = ''.join([chr((ptr + i).dereference()) for i in range(length)])
    return string + error_message


def dereference_member(obj):
    """Dereferences a pointer held by a Member.

    obj must be a gdb.Value. The type is not one of the cppgc Member types, the
    original object is returned back."""
    obj_typename = obj.type.strip_typedefs().name
    if not obj_typename.startswith('cppgc::internal::BasicMember'):
        return obj

    inner_type = obj.type.template_argument(0)
    decompressed_ptr = (gdb.parse_and_eval(
        "_cppgc_internal_Uncompress_Member((void*){})".format(
            obj.address)).cast(inner_type.pointer()))

    # The sentinel pointer is used when stored in a hash set to indicate
    # deleted entries.
    sentinel = gdb.parse_and_eval('cppgc::kSentinelPointer')['kSentinelValue']
    ptr_value = int(decompressed_ptr)
    if ptr_value == 0 or ptr_value == int(sentinel):
        return None

    return decompressed_ptr.dereference()


class StringPrinter(object):
    "Shared code between different string-printing classes"

    def __init__(self, val):
        self.val = val

    def display_hint(self):
        return 'string'


class UCharStringPrinter(StringPrinter):
    "Print a UChar*; we must guess at the length"

    def to_string(self):
        return ustring_to_string(self.val)


class LCharStringPrinter(StringPrinter):
    "Print a LChar*; we must guess at the length"

    def to_string(self):
        return lstring_to_string(self.val)


class WTFAtomicStringPrinter(StringPrinter):
    "Print a WTF::AtomicString"

    def to_string(self):
        return self.val['string_']


class WTFStringImplPrinter(StringPrinter):
    "Print a WTF::StringImpl"

    def get_length(self):
        return self.val['length_']

    def to_string(self):
        chars_start = self.val.address + 1
        if self.is_8bit():
            return lstring_to_string(
                chars_start.cast(gdb.lookup_type('char').pointer()),
                self.get_length())
        return ustring_to_string(
            chars_start.cast(gdb.lookup_type('UChar').pointer()),
            self.get_length())

    def is_8bit(self):
        return int(str(self.val['hash_and_flags_'])) % 2


class WTFStringPrinter(StringPrinter):
    "Print a WTF::String"

    def stringimpl_ptr(self):
        return self.val['impl_']['ptr_']

    def get_length(self):
        if not self.stringimpl_ptr():
            return 0
        return WTFStringImplPrinter(
            self.stringimpl_ptr().dereference()).get_length()

    def to_string(self):
        if not self.stringimpl_ptr():
            return '(null)'
        return self.stringimpl_ptr().dereference()


class blinkKURLPrinter(StringPrinter):
    "Print a blink::KURL"

    def to_string(self):
        return WTFAtomicStringPrinter(self.val['string_']).to_string()


class blinkLayoutUnitPrinter:
    "Print a blink::LayoutUnit"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "%.14gpx" % (self.val['value_'] / 64.0)


class blinkFixedPointPrinter:
    "Print a blink::FixedPoint (LayoutUnit, etc.)"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "%.14gpx" % (float(self.val['value_']) /
                            float(self.val['kFixedPointDenominator']))


class blinkLayoutPointPrinter:
    "Print a blink::LayoutPoint"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'LayoutPoint(%s, %s)' % (blinkLayoutUnitPrinter(
            self.val['x_']).to_string(), blinkLayoutUnitPrinter(
                self.val['y_']).to_string())


class blinkQualifiedNamePrinter(StringPrinter):
    "Print a blink::QualifiedName"

    def __init__(self, val):
        super(blinkQualifiedNamePrinter, self).__init__(val)
        self.prefix_length = 0
        self.length = 0
        if self.val['impl_']:
            self.prefix_printer = WTFStringPrinter(
                self.val['impl_']['ptr_']['prefix_']['string_'])
            self.local_name_printer = WTFStringPrinter(
                self.val['impl_']['ptr_']['local_name_']['string_'])
            self.prefix_length = self.prefix_printer.get_length()
            if self.prefix_length > 0:
                self.length = (self.prefix_length + 1 +
                               self.local_name_printer.get_length())
            else:
                self.length = self.local_name_printer.get_length()

    def get_length(self):
        return self.length

    def to_string(self):
        if self.get_length() == 0:
            return "(null)"
        else:
            if self.prefix_length > 0:
                return (self.prefix_printer.to_string() + ":" +
                        self.local_name_printer.to_string())
            else:
                return self.local_name_printer.to_string()


class BlinkPixelsAndPercentPrinter:
    "Print a blink::PixelsAndPercent value"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "(%gpx, %g%%)" % (self.val['pixels'], self.val['percent'])


class BlinkLengthPrinter:
    """Print a blink::Length."""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        ltype = self.val['type_']
        val = self.val['value_']

        quirk = ''
        if self.val['quirk_']:
            quirk = ', quirk=true'

        if ltype == 0:
            return 'Length(Auto)'
        if ltype == 1:
            return 'Length(%g%%, Percent%s)' % (val, quirk)
        if ltype == 2:
            return 'Length(%g, Fixed%s)' % (val, quirk)
        if ltype == 3:
            return 'Length(MinContent)'
        if ltype == 4:
            return 'Length(MaxContent)'
        if ltype == 5:
            return 'Length(MinIntrinsic)'
        if ltype == 6:
            return 'Length(Stretch)'
        if ltype == 7:
            return 'Length(FitContent)'
        if ltype == 8:
            # Would like to print pixelsAndPercent() but can't call member
            # functions - https://sourceware.org/bugzilla/show_bug.cgi?id=13326
            return 'Length(Calculated)'
        if ltype == 9:
            return 'Length(ExtendToZoom)'
        if ltype == 10:
            return 'Length(DeviceWidth)'
        if ltype == 11:
            return 'Length(DeviceHeight)'
        if ltype == 12:
            return 'Length(MaxSizeNone)'
        return 'Length(unknown type %i)' % ltype


class WTFVectorPrinter:
    """Pretty Printer for a WTF::Vector.

    The output of this pretty printer is similar to the output of std::vector's
    pretty printer, which is bundled in gcc.

    Example gdb session should look like:
    (gdb) p v
    $3 = WTF::Vector of length 7, capacity 16 = {7, 17, 27, 37, 47, 57, 67}
    (gdb) set print elements 3
    (gdb) p v
    $6 = WTF::Vector of length 7, capacity 16 = {7, 17, 27...}
    (gdb) set print array
    (gdb) p v
    $7 = WTF::Vector of length 7, capacity 16 = {
      7,
      17,
      27
      ...
    }
    (gdb) set print elements 200
    (gdb) p v
    $8 = WTF::Vector of length 7, capacity 16 = {
      7,
      17,
      27,
      37,
      47,
      57,
      67
    }
    """

    class Iterator:
        def __init__(self, start, finish):
            self.item = start
            self.finish = finish
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.item == self.finish:
                raise StopIteration
            count = self.count
            self.count += 1
            element = dereference_member(self.item.dereference())
            self.item += 1
            return ('[%d]' % count, element)

        # Python version < 3 compatibility:
        def next(self):
            return self.__next__()

    def __init__(self, val):
        self.val = val

    def children(self):
        start = self.val['buffer_']
        return self.Iterator(start, start + self.val['size_'])

    def to_string(self):
        return ('%s of length %d, capacity %d' %
                ('WTF::Vector', self.val['size_'], self.val['capacity_']))

    def display_hint(self):
        return 'array'


class WTFHashTablePrinter:
    """Pretty printer for a WTF::HashTable.

    The output of this pretty printer is similar to the output of
    std::unordered_map's pretty printer, which is bundled with gcc.

    An example gdb session should look like:
    (gdb) print m
    $1 = {impl_ = WTF::HashTable with 2 elements = {
        ["a-start"] = 0, ["a-end"] = 1}}
    """

    class Iterator:

        def __init__(self, start, finish, is_keyval):
            self.item = start
            self.finish = finish
            self.count = 0
            self.is_keyval = is_keyval

        def __iter__(self):
            return self

        def __next__(self):
            # Loop until we find a non-empty bucket.
            while True:
                if self.item == self.finish:
                    raise StopIteration
                count = self.count
                self.count += 1
                element = dereference_member(self.item.dereference())
                self.item += 1

                # If the bucket is not empty, return it.
                # TODO(bokan): This doesn't account for HashTraits of the table
                # so may print empty/deleted buckets, depending on the type.
                if self.is_keyval:
                    derefed_key = dereference_member(element['key'])
                    if derefed_key:
                        derefed_val = dereference_member(element['value'])
                        pretty = '[%s] = %s' % (derefed_key, derefed_val)
                        return ('[%d]' % count, pretty)
                else:
                    if element:
                        pretty = '%s' % (element)
                        return ('[%d]' % count, pretty)

    def __init__(self, val):
        self.val = val

    def children(self):
        start = self.val['table_']
        # HashSets use a HashTable where the value and key are the same so the
        # iteration needs to know whether to look for an explicit key or not.
        extractor_name = self.val.type.template_argument(2).name
        is_keyval = extractor_name != 'WTF::IdentityExtractor'
        return self.Iterator(start, start + self.val['table_size_'], is_keyval)

    def to_string(self):
        return ('%s with %d elements' %
                ('WTF::HashTable', self.val['key_count_']))

    def display_hint(self):
        return 'array'


# Copied from //tools/gdb/gdb_chrome.py
def typed_ptr(ptr):
    """Prints a pointer along with its exact type.

    By default, gdb would print just the address, which takes more
    steps to interpret.
    """
    # Returning this as a cast expression surrounded by parentheses
    # makes it easier to cut+paste inside of gdb.
    return '((%s)%s)' % (ptr.dynamic_type, ptr)


class BlinkDataRefPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'DataRef(%s)' % (str(self.val['data_']))


class BlinkJSONValuePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        s = str(
            gdb.parse_and_eval(
                "((blink::JSONValue*) %s)->ToPrettyJSONString().Utf8(0)" %
                self.val.address))
        return s.replace("\\n", "\n").replace('\\"', '"')


class CcPaintOpBufferPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return gdb.parse_and_eval(
            "blink::RecordAsJSON(*((cc::PaintOpBuffer*) %s))" %
            self.val.address)


def add_pretty_printers():
    pretty_printers = (
        (re.compile("^WTF::Vector<.*>$"), WTFVectorPrinter),
        (re.compile("^WTF::HashTable<.*>$"), WTFHashTablePrinter),
        (re.compile("^WTF::AtomicString$"), WTFAtomicStringPrinter),
        (re.compile("^WTF::String$"), WTFStringPrinter),
        (re.compile("^WTF::StringImpl$"), WTFStringImplPrinter),
        (re.compile("^blink::FixedPoint<.*>$"), blinkFixedPointPrinter),
        (re.compile("^blink::KURL$"), blinkKURLPrinter),
        (re.compile("^blink::LayoutUnit$"), blinkLayoutUnitPrinter),
        (re.compile("^blink::LayoutPoint$"), blinkLayoutPointPrinter),
        (re.compile("^blink::QualifiedName$"), blinkQualifiedNamePrinter),
        (re.compile("^blink::PixelsAndPercent$"),
         BlinkPixelsAndPercentPrinter),
        (re.compile("^blink::Length$"), BlinkLengthPrinter),
        (re.compile("^blink::DataRef<.*>$"), BlinkDataRefPrinter),
        (re.compile("^blink::JSONValue$"), BlinkJSONValuePrinter),
        (re.compile("^blink::JSONBasicValue$"), BlinkJSONValuePrinter),
        (re.compile("^blink::JSONString$"), BlinkJSONValuePrinter),
        (re.compile("^blink::JSONObject$"), BlinkJSONValuePrinter),
        (re.compile("^blink::JSONArray$"), BlinkJSONValuePrinter),
        (re.compile("^cc::PaintOpBuffer$"), CcPaintOpBufferPrinter),
    )

    def lookup_function(val):
        """Function used to load pretty printers; will be passed to GDB."""
        type = val.type
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target()
        type = type.unqualified().strip_typedefs()
        tag = type.tag
        if tag:
            for function, pretty_printer in pretty_printers:
                if function.search(tag):
                    return pretty_printer(val)

        if type.code == gdb.TYPE_CODE_PTR:
            name = str(type.target().unqualified())
            if name == 'UChar':
                return UCharStringPrinter(val)
            if name == 'LChar':
                return LCharStringPrinter(val)
        return None

    gdb.pretty_printers.append(lookup_function)


add_pretty_printers()


class PrintPathToRootCommand(gdb.Command):
    """Command for printing Blink Node trees.

    Usage: printpathtoroot variable_name"""

    def __init__(self):
        super(PrintPathToRootCommand, self).__init__(
            "printpathtoroot", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE)

    def invoke(self, arg, from_tty):
        element_type = gdb.lookup_type('blink::Element')
        node_type = gdb.lookup_type('blink::Node')
        frame = gdb.selected_frame()
        try:
            val = gdb.Frame.read_var(frame, arg)
        except:
            print("No such variable, or invalid type")
            return

        target_type = str(val.type.target().strip_typedefs())
        if target_type == str(node_type):
            stack = []
            while val:
                stack.append([
                    val,
                    val.cast(element_type.pointer()).dereference()['tag_name_']
                ])
                val = val.dereference()['parent_']

            padding = ''
            while len(stack) > 0:
                pair = stack.pop()
                print(padding, pair[1], pair[0])
                padding = padding + '  '
        else:
            print(
                'Sorry: I don\'t know how to deal with %s yet.' % target_type)


PrintPathToRootCommand()
