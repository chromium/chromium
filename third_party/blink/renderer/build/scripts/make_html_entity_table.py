#!/usr/bin/env python
# Copyright (c) 2010 Google Inc. All rights reserved.
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
"""Generates html_entity_table.cc from html_entity_names.csv.

The generated file contains:
  * kStaticEntityStringStorage[] : a single LChar array containing every
    entity name as a (packed) substring. Entries share storage where they
    overlap (a later entity may reuse the suffix of an earlier one).
  * kStaticEntityTable[]         : one HTMLEntityTableEntry per entity,
    pointing into kStaticEntityStringStorage and carrying the 1-2 code
    points the entity expands to.
  * kUppercaseOffset / kLowercaseOffset : per-starting-letter index ranges
    so that the parser can look up only entries starting with a given char.

The output layout is described in templates/html_entity_table.cc.tmpl.
"""

import csv
import os.path
import sys

import template_expander

ENTITY = 0
VALUE = 1


def convert_value_to_int(value):
    if not value:
        return "0"
    assert value[0] == "U"
    assert value[1] == "+"
    return "0x" + value[2:]


def check_ascii(entity_string):
    for ch in entity_string:
        code = ord(ch)
        assert 0 <= code <= 127, (
            ch + " is not ASCII. Need to change type " +
            "of storage from LChar to UChar to support " + "this entity.")


def build_storage_and_offsets(entries):
    """Pack entity names into shared storage chunks.

    Returns chunks, a list[str] where each chunk is the bytes added to the
    packed storage for one entity (only the part not already covered by
    previous storage). Entries that fully reuse existing storage do not
    add a chunk.

    As a side effect, appends the computed offset to each entry list
    (making it length 3: [name, value, offset]).
    """
    chunks = []
    all_data = ""
    entity_offset = 0
    for entry in entries:
        check_ascii(entry[ENTITY])
        # Reuse substrings from earlier entries when possible. This saves
        # 1-2000 characters, but it's O(n^2) and not very smart. The
        # optimal solution has to solve the "Shortest Common Superstring"
        # problem and that is NP-Complete or worse.
        #
        # This would be even more efficient if we didn't store the
        # semi-colon in the array but as a bit in the entry.
        entity = entry[ENTITY]
        already_existing_offset = all_data.find(entity)
        if already_existing_offset != -1:
            this_offset = already_existing_offset
        else:
            # Try the end of the existing storage and see if we can reuse
            # that as the prefix of the new entity.
            data_to_add = entity
            this_offset = entity_offset
            for truncated_len in range(len(entity) - 1, 0, -1):
                if all_data.endswith(entity[:truncated_len]):
                    data_to_add = entity[truncated_len:]
                    this_offset = entity_offset - truncated_len
                    break

            chunks.append(data_to_add)
            all_data += data_to_add
            entity_offset += len(data_to_add)
        assert len(
            entry) == 2, "We will use slot [2] in the list for the offset."
        assert this_offset < 32768  # Stored in a 16 bit short.
        entry.append(this_offset)
    return chunks


def build_template_params(input_path):
    with open(input_path) as html_entity_names_file:
        entries = list(csv.reader(html_entity_names_file))

    entries.sort(key=lambda entry: entry[ENTITY])
    assert len(entries) > 0, "Code assumes a non-empty entity array."

    chunks = build_storage_and_offsets(entries)

    # Pre-format each chunk into the literal C++ source line it should
    # become inside kStaticEntityStringStorage[]. Doing this in Python
    # (rather than in the template) keeps the template free of fragile
    # whitespace-control gymnastics around inter-chunk commas and the
    # closing "};" of the array.
    storage_lines = [', '.join("'%s'" % c for c in chunk) for chunk in chunks]
    # Commas separate chunks; the final chunk is followed by the closing
    # brace on the same line, matching the historical hand-rolled layout.
    storage_lines = [line + ',' for line in storage_lines[:-1]
                     ] + [storage_lines[-1] + '};']

    # Build the per-starting-letter index used by EntriesStartingWith().
    index = {}
    for offset, entry in enumerate(entries):
        starting_letter = entry[ENTITY][0]
        if starting_letter not in index:
            index[starting_letter] = offset

    # kUppercaseOffset has one entry per upper letter A..Z plus the index
    # of the first lowercase entry (used as the upper bound for 'Z').
    uppercase_offsets = [
        index[chr(letter)] for letter in range(ord('A'),
                                               ord('Z') + 1)
    ]
    uppercase_offsets.append(index['a'])

    # kLowercaseOffset has one entry per lower letter a..z plus the total
    # entry count (used as the upper bound for 'z').
    lowercase_offsets = [
        index[chr(letter)] for letter in range(ord('a'),
                                               ord('z') + 1)
    ]
    lowercase_offsets.append(len(entries))

    template_entries = []
    for entry in entries:
        values = entry[VALUE].split(' ')
        assert len(values) <= 2, values
        template_entries.append({
            'name':
            entry[ENTITY],
            'first_value':
            convert_value_to_int(values[0]),
            'second_value':
            convert_value_to_int(values[1] if len(values) >= 2 else ""),
            'offset':
            entry[2],
            'length':
            len(entry[ENTITY]),
        })

    return {
        'storage_lines': storage_lines,
        'entries': template_entries,
        'uppercase_offsets': uppercase_offsets,
        'lowercase_offsets': lowercase_offsets,
    }


def main():
    program_name = os.path.basename(__file__)
    if len(sys.argv) < 4 or sys.argv[1] != "-o":
        sys.stderr.write("Usage: %s -o OUTPUT_FILE INPUT_FILE\n" %
                         program_name)
        exit(1)

    output_path = sys.argv[2]
    input_path = sys.argv[3]

    params = build_template_params(input_path)
    rendered = template_expander.apply_template(
        'templates/html_entity_table.cc.tmpl', params)

    with open(output_path, "w") as output_file:
        output_file.write(rendered)


if __name__ == "__main__":
    main()
