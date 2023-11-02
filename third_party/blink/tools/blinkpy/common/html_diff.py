# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility for outputting a HTML diff of two multi-line strings.

The main purpose of this utility is to show the difference between
text baselines (-expected.txt files) and actual text results.

Note, in the standard library module difflib, there is also a HtmlDiff class,
although it outputs a larger and more complex HTML table than we need.
"""

import difflib

try:
    from cgi import escape
except ImportError:
    # cgi.escape is deprecated in Python3
    from html import escape

_TEMPLATE = """<html>
<head>
<style>
table { white-space: pre-wrap; font-family: monospace; border-collapse: collapse; }
th { color: #444; background: #eed; text-align: right; vertical-align: baseline; padding: 1px 4px 1px 4px; }
.del { background: #faa; }
.add { background: #afa; }
</style>
</head>
<body><table>%s</table></body>
</html>
"""


def html_diff(a_text, b_text):
    """Returns a diff between two strings as HTML."""
    # Diffs can be between multiple text files of different encodings
    # so we always want to deal with them as byte arrays, not unicode strings.
    assert isinstance(a_text, str)
    assert isinstance(b_text, str)
    a_lines = a_text.splitlines(True)
    b_lines = b_text.splitlines(True)
    return _TEMPLATE % HtmlDiffGenerator().generate_tbody(a_lines, b_lines)


class HtmlDiffGenerator(object):
    def __init__(self):
        self.a_line_no = None
        self.b_line_no = None
        self.a_lines_len = None

    def generate_tbody(self, a_lines, b_lines):
        self.a_line_no = 0
        self.b_line_no = 0
        self.a_lines_len = len(a_lines)
        self.b_lines_len = len(b_lines)
        matcher = difflib.SequenceMatcher(None, a_lines, b_lines)
        output = []
        for tag, a_start, a_end, b_start, b_end in matcher.get_opcodes():
            output.append(
                self._format_chunk(tag, a_lines[a_start:a_end],
                                   b_lines[b_start:b_end]))
        return ''.join(output)

    def _format_chunk(self, tag, a_chunk, b_chunk):
        if tag == 'delete':
            return self._format_delete(a_chunk)
        if tag == 'insert':
            return self._format_insert(b_chunk)
        if tag == 'replace':
            return self._format_delete(a_chunk) + self._format_insert(b_chunk)
        assert tag == 'equal'
        return self._format_equal(a_chunk)

    def _format_equal(self, common_chunk):
        output = ''
        if len(common_chunk) <= 7:
            for line in common_chunk:
                output += self._format_equal_line(line)
        else:
            # Do not show context lines at the beginning of the file.
            if self.a_line_no == 0 and self.b_line_no == 0:
                self.a_line_no += 3
                self.b_line_no += 3
            else:
                for line in common_chunk[0:3]:
                    output += self._format_equal_line(line)
            self.a_line_no += len(common_chunk) - 6
            self.b_line_no += len(common_chunk) - 6
            output += '<tr><td colspan=3>\n\n</tr>'
            # Do not show context lines at the end of the file.
            if self.a_line_no + 3 != self.a_lines_len or self.b_line_no + 3 != self.b_lines_len:
                for line in common_chunk[len(common_chunk) -
                                         3:len(common_chunk)]:
                    output += self._format_equal_line(line)
        return output

    def _format_equal_line(self, line):
        self.a_line_no += 1
        self.b_line_no += 1
        return '<tr><th>%d<th>%d<td>%s</tr>' % (self.a_line_no, self.b_line_no,
                                                escape(line))

    def _format_insert(self, chunk):
        output = ''
        for line in chunk:
            self.b_line_no += 1
            output += '<tr><th><th>%d<td class="add">%s</tr>' % (
                self.b_line_no, escape(line))
        return output

    def _format_delete(self, chunk):
        output = ''
        for line in chunk:
            self.a_line_no += 1
            output += '<tr><th>%d<th><td class="del">%s</tr>' % (
                self.a_line_no, escape(line))
        return output
