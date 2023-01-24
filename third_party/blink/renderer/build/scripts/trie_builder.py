# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict


def _single_trie(string_to_value_pairs, index):
    """Build a single trie from a dict of input string to output values.

    This function assumes that all of the strings in
    string_to_value_pairs have the same length.
    The input
    {'abcd': 'ABCD', 'adef': 'ADEF', 'adeg': 'ADEG'}
    creates a trie like this:
    {
     'a' : {
      'b': {'cd' : "ABCD"},
      'd' : {
       'e' : {
        'f' : {'': "ADEF"},
        'g' : {'': "ADEG"},
       },
      },
     },
    }
    """
    dicts_by_indexed_letter = defaultdict(list)
    for string, value in string_to_value_pairs:
        dicts_by_indexed_letter[string[index]].append((string, value))

    output = {}
    for char, d in dicts_by_indexed_letter.items():
        if len(d) == 1:
            string = d[0][0]
            value = d[0][1]
            output[char] = {string[index + 1:]: value}
        else:
            output[char] = _single_trie(d, index + 1)

    return output


def trie_list_by_str_length(str_to_return_value_dict):
    """Make a list of tries from a dict of input string to output value.

    All strings should be all lower case.
    """
    dicts_by_length = defaultdict(list)
    for string, value in str_to_return_value_dict.items():
        dicts_by_length[len(string)].append((string, value))

    output = []
    for length, pairs in sorted(dicts_by_length.items()):
        output.append((length, _single_trie(sorted(pairs), 0)))

    return output
