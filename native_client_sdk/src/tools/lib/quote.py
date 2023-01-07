#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

def quote(input_str, specials, escape='\\'):
  """Returns a quoted version of |str|, where every character in the
  iterable |specials| (usually a set or a string) and the escape
  character |escape| is replaced by the original character preceded by
  the escape character."""

  assert len(escape) == 1

  # Since escape is used in replacement pattern context, so we need to
  # ensure that it stays a simple literal and cannot affect the \1
  # that will follow it.
  if escape == '\\':
    escape = '\\\\'
  if len(specials) > 0:
    return re.sub(r'(' + r'|'.join(specials)+r'|'+escape + r')',
                  escape + r'\1', input_str)
  return re.sub(r'(' + escape + r')', escape + r'\1', input_str)


def unquote(input_str, specials, escape='\\'):
  """Splits the input string |input_str| where special characters in
  the input |specials| are, if not quoted by |escape|, used as
  delimiters to split the string.  The returned value is a list of
  strings of alternating non-specials and specials used to break the
  string. The list will always begin with a possibly empty string of
  non-specials, but may end with either specials or non-specials."""

  assert len(escape) == 1

  out = []
  cur_out = []
  cur_special = False
  lit_next = False
  for c in input_str:
    if cur_special:
      lit_next = (c == escape)
      if c not in specials or lit_next:
        cur_special = False
        out.append(''.join(cur_out))
        if not lit_next:
          cur_out = [c]
        else:
          cur_out = []
      else:
        cur_out.append(c)
    else:
      if lit_next:
        cur_out.append(c)
        lit_next = False
      else:
        lit_next = c == escape
        if c not in specials:
          if not lit_next:
            cur_out.append(c)
        else:
          out.append(''.join(cur_out))
          cur_out = [c]
          cur_special = True
  out.append(''.join(cur_out))
  return out
