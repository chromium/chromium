# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prettifies 'git diff' output.

prettify_diff() takes a diff string, and returns an HTML string decorating the
diff.

This code doesn't support other diff commands such as "diff" and "svn diff".
"""

import base64
import difflib
import mimetypes
import re
import six
import zlib

if six.PY2:
    import cgi
else:
    import html as cgi

# The style below is meant to be similar to PolyGerrit.
_LEADING_HTML = """<!DOCTYPE html>
<meta charset="UTF-8">
<style>
body {
  background: white;
  font-family: "Roboto Mono", Menlo, "Lucida Console", Monaco, monospace;
}
table {
  border-collapse: collapse;
  border-spacing: 0;
  width: 100%;
  margin-top: 1em;
}
td { white-space: pre-wrap; font-size: 14px; }
.fileheader { position: sticky; top: 0px; }
.fileheader-container {
  background: #eee;
  border-bottom: 1px solid #ddd;
  border-top: 1px solid #ddd;
  box-sizing: border-box;
  display: flex;
  line-height: 2.25em;
  padding: 0.2em 1rem 0.2em 1rem;
}
.filename { flex-grow: 1; }
.fileheader button { flex-grow: 0; width: 2.25em; }
.rename { color: #999999; display: block; }
.fileinfo { background: #fafafa; color: #3a66d9; }
.filehooter div { border-top: 1px solid #ddd; }
.hunkheader { background: rgb(255, 247, 212); color: #757575; }
.lineno {
  background: #fafafa;
  box-sizing: border-box;
  color: #666;
  padding: 0 0.5em;
  text-align: right;
  user-select: none;
  vertical-align: top;
  width: 94px;
}
.emptylineno { box-sizing: border-box;  user-select: none; width: 94px; }
.code { border-left: 1px solid #ddd; word-break: break-all; }
.del { background: #ffeeee; }
.del.strong { background: #ffcaca; }
.add { background: #eeffee; }
.add.strong { background: #caffca; }
.binary { padding: 8px; border-left: 1px solid #ddd; }
pre { white-space: pre-wrap; font-size: 14px; }
.hidden { display: none; }
</style>
<body>
<script>
function toggleFollowingRows(button) {
  button.textContent = button.textContent == '\\u25B2' ? '\\u25BC' : '\\u25B2';
  let parent = button;
  while (parent && parent.tagName != 'TR') {
    parent = parent.parentNode;
  }
  if (!parent)
    return;
  for (let next = parent.nextSibling; next; next = next.nextSibling) {
    if (next.tagName == 'TR')
      next.classList.toggle('hidden')
  }
}
</script>
"""


def prettify_diff(diff_str):
    diff_lines = diff_str.split('\n')
    # List of DiffFile instances
    diff_files = []

    diff_file, diff_lines = DiffFile.parse(diff_lines)
    while diff_file:
        diff_files.append(diff_file)
        diff_file, diff_lines = DiffFile.parse(diff_lines)

    result_html = _LEADING_HTML
    for diff_file in diff_files:
        result_html += diff_file.prettify()

    # If diff_lines still has unconsumed lines, this code has a bug or the input
    # diff is broken. We show the raw diff in such case.
    if diff_lines:
        result_html += '<pre>'
        for line in diff_lines:
            result_html += cgi.escape(line) + '\n'
        result_html += '</pre>'

    return result_html + '</body>\n'


class DiffFile(object):
    """Represents diff for a single file.

    An instance of this class contains one of the following:
    - Text hunks
    - Two binary hunks
    - Meta information
    """
    LINK_BASE_URL = 'https://chromium.googlesource.com/chromium/src/+/main/'

    def __init__(self,
                 old_name,
                 new_name,
                 hunks=None,
                 binaries=None,
                 info=None):
        assert old_name or new_name
        assert bool(hunks) + bool(binaries) + bool(info) == 1
        self._old_name = old_name
        self._new_name = new_name
        self._hunks = hunks
        self._binaries = binaries
        self._info = info

    def prettify(self):
        status = 'M'
        pretty_name = self._linkify(self._new_name)
        additional_info = ''
        if self._old_name == '':
            status = 'A'
            pretty_name = cgi.escape(self._new_name)
        elif self._new_name == '':
            status = 'D'
            pretty_name = self._linkify(self._old_name)
        elif self._old_name != self._new_name:
            status = 'R'
            pretty_name = cgi.escape(self._new_name)
            additional_info = (
                '\n<span class=rename>Renamed from {}</span>'.format(
                    self._linkify(self._old_name)))

        result_html = (
            '\n<table>\n<tr><td colspan=3 class=fileheader>'
            '<div class=fileheader-container>'
            '<div class=filename>' + status + ' ' + pretty_name +
            additional_info + '</div>'
            '<button type=button onclick="toggleFollowingRows(this);">&#x25B2;</button>'
            '</div></tr>')

        if self._hunks:
            for hunk in self._hunks:
                result_html += hunk.prettify()
        elif self._info:
            result_html += '<tr><td colspan=3 class=fileinfo>{}</tr>'.format(
                cgi.escape('\n'.join(self._info)))
        else:
            old_binary, new_binary = self._binaries  # pylint: disable=unpacking-non-sequence
            if self._old_name and old_binary:
                result_html += old_binary.prettify(
                    self._mime_from_name(self._old_name), 'del')
            if self._new_name and new_binary:
                result_html += new_binary.prettify(
                    self._mime_from_name(self._new_name), 'add')
        return result_html + '<tr><td colspan=3 class=filehooter><div></div></table>\n'

    def _linkify(self, name):
        return '<a href="{url}" target="_new">{anchor}</a>'.format(
            url=DiffFile.LINK_BASE_URL + cgi.escape(name),
            anchor=cgi.escape(name))

    def _mime_from_name(self, name):
        mime_type, _ = mimetypes.guess_type(name)
        return mime_type if mime_type else 'application/octet-stream'

    @staticmethod
    def parse(lines):
        """Parses diff lines, and creates a DiffFile instance.

        Finds a file diff header, creates a single DiffFile instance, and
        returns a tuple of the DiffFile instance and unconsumed lines. If a file
        diff isn't found, (None, lines) is returned.
        """
        diff_command_re = r'diff (?:-[^ ]+ )*a/([^ ]+) b/([^ ]+)'
        old_name = None
        new_name = None
        info_lines = None
        found_diff_command_line = False
        for i, line in enumerate(lines):
            if not found_diff_command_line:
                match = re.match(diff_command_re, line)
                if not match:
                    continue
                old_name = match.group(1)
                new_name = match.group(2)
                info_lines = []
                found_diff_command_line = True
                continue

            match = re.match(r'(GIT binary patch|--- ([^ ]+).*)', line)
            if match:
                if match.group(0) == 'GIT binary patch':
                    return DiffFile._parse_binaries(lines[i + 1:], old_name,
                                                    new_name)
                return DiffFile._parse_text_hunks(lines[i:], old_name,
                                                  new_name)

            index_match = re.match(r'^index ([0-9a-f]+)\.\.([0-9a-f]+).*',
                                   line)
            if index_match:
                # Adjusts old_name and new_name for file addition/removal.
                old_name, new_name = DiffFile._adjust_names(
                    index_match, old_name, new_name)
                continue

            diff_match = re.match(diff_command_re, line)
            if diff_match:
                # There are no hunks. Renaming without any modification,
                # or adding/removing an empty file.
                return (DiffFile(old_name, new_name, info=info_lines),
                        lines[i:])

            # File mode, rename summary, etc.
            info_lines.append(line)

        if found_diff_command_line and info_lines:
            return (DiffFile(old_name, new_name, info=info_lines), [])
        return (None, lines)

    @staticmethod
    def _parse_binaries(lines, old_name, new_name):
        new_binary, remaining_lines = BinaryHunk.parse(lines)
        old_binary, remaining_lines = BinaryHunk.parse(remaining_lines)
        return (DiffFile(
            old_name, new_name, binaries=(old_binary, new_binary)),
                remaining_lines)

    @staticmethod
    def _parse_text_hunks(lines, old_name, new_name):
        line = lines[0]
        if len(lines) < 2:
            raise ValueError('"+++ " line is missing after "{}"'.format(line))
        next_line = lines[1]
        if not next_line.startswith('+++ '):
            raise ValueError('"+++ " line is missing after "{}"'.format(line))
        hunks, remaining_lines = DiffHunk.parse(lines[2:])
        return (DiffFile(old_name, new_name, hunks=hunks), remaining_lines)

    @staticmethod
    def _adjust_names(match, old_name, new_name):
        old_index = match.group(1)
        new_index = match.group(2)
        if old_index and re.match(r'^0+$', old_index):
            old_name = ''
        if new_index and re.match(r'^0+$', new_index):
            new_name = ''
        return (old_name, new_name)


class DiffHunk(object):
    """Represents a single text hunk, starting with '@@ -d,d +d,d @@'.

    This class also has code to detect character-level diff.
    """

    def __init__(self, old_start, new_start, context, lines):
        self._old_start = old_start
        self._new_start = new_start
        self._context = ''
        if context:
            self._context = context
            if self._context.startswith(' '):
                self._context = self._context[1:]
        self._lines = lines
        # _annotations is a list of None or a list of tuples.
        # A tuple consists of start index and end index, and it represents a
        # modified part of a line, which should be highlighted in the pretty
        # diff.
        self._annotations = [None for _ in self._lines]
        for deleted_index, inserted_index in self._find_operations(
                self._lines):
            DiffHunk._annotate_character_diff(
                self._lines, deleted_index, inserted_index, self._annotations)

    @staticmethod
    def _find_operations(lines):
        """Finds 'operations' in the hunk.

        A hunk contains one or more operations, and an operation is one of the
        followings:
          - Replace operation: '-' lines, followed by '+' lines
          - Delete operation: '-' lines, not followed by '+' lines
          - Insertion operation: '+' lines
        """
        # List of tuples which consist of (list of '-' line index, list of '+' line index)
        operations = []
        inserted_index = []
        deleted_index = []
        for i, line in enumerate(lines):
            if line[0] == ' ':
                if deleted_index or inserted_index:
                    operations.append((deleted_index, inserted_index))
                    deleted_index = []
                    inserted_index = []
            elif line[0] == '-':
                if inserted_index:
                    operations.append((deleted_index, inserted_index))
                    deleted_index = []
                    inserted_index = []
                deleted_index.append(i)
            else:
                assert line[0] == '+'
                inserted_index.append(i)
        if deleted_index or inserted_index:
            operations.append((deleted_index, inserted_index))
        return operations

    @staticmethod
    def _annotate_character_diff(lines, deleted_index, inserted_index,
                                 annotations):
        assert len(lines) == len(annotations)
        if not deleted_index:
            for i in inserted_index:
                annotations[i] = [(0, len(lines[i]) - 1)]
            return

        if not inserted_index:
            for i in deleted_index:
                annotations[i] = [(0, len(lines[i]) - 1)]
            return

        deleted_str = ''.join([lines[i][1:] for i in deleted_index])
        inserted_str = ''.join([lines[i][1:] for i in inserted_index])
        matcher = difflib.SequenceMatcher(None, deleted_str, inserted_str)
        for tag, d_start, d_end, i_start, i_end in matcher.get_opcodes():
            if tag == 'delete':
                DiffHunk._annotate(lines, deleted_index[0], d_start, d_end,
                                   annotations)
            elif tag == 'insert':
                DiffHunk._annotate(lines, inserted_index[0], i_start, i_end,
                                   annotations)
            elif tag == 'replace':
                DiffHunk._annotate(lines, deleted_index[0], d_start, d_end,
                                   annotations)
                DiffHunk._annotate(lines, inserted_index[0], i_start, i_end,
                                   annotations)

    @staticmethod
    def _annotate(lines, index, start, end, annotations):
        assert index < len(lines)
        while index < len(lines):
            line_len = len(lines[index]) - 1
            if line_len == 0 and start == 0:
                annotations[index] = [(0, 0)]
                index += 1
                continue
            if start >= line_len:
                start -= line_len
                end -= line_len
                index += 1
                continue

            if not annotations[index]:
                annotations[index] = []
            annotations[index].append((start, min(line_len, end)))
            if end > line_len:
                start = 0
                end -= line_len
                index += 1
                continue
            else:
                break

    def prettify_code(self, index, klass):
        line = self._lines[index][1:]
        annotation = self._annotations[index]
        if not annotation:
            return '<td class="code {klass}">{code}'.format(
                klass=klass, code=cgi.escape(line))

        start, end = annotation[0]
        if start == 0 and end == len(line):
            return '<td class="code {klass} strong">{code}'.format(
                klass=klass, code=cgi.escape(line))

        i = 0
        result_html = '<td class="code {}">'.format(klass)
        for start, end in annotation:
            result_html += cgi.escape(line[i:start])
            result_html += '<span class="{} strong">'.format(klass)
            result_html += cgi.escape(line[start:end])
            result_html += '</span>'
            i = end
        return result_html + cgi.escape(line[i:])

    def prettify(self):
        result_html = ('<tr><td class=hunkheader>@@<td class=hunkheader>@@'
                       '<td class=hunkheader>{}</tr>\n').format(
                           cgi.escape(self._context))
        old_lineno = self._old_start
        new_lineno = self._new_start
        for i, line in enumerate(self._lines):
            if line[0] == ' ':
                result_html += (
                    '<tr><td class=lineno>{old_lineno}<td '
                    'class=lineno>{new_lineno}<td class=code>{code}'
                    '</tr>\n').format(
                        old_lineno=old_lineno,
                        new_lineno=new_lineno,
                        code=cgi.escape(line[1:]))
                old_lineno += 1
                new_lineno += 1
            elif line[0] == '-':
                result_html += '<tr><td class=lineno>{lineno}<td class=emptylineno>{code}</tr>\n'.format(
                    lineno=old_lineno, code=self.prettify_code(i, 'del'))
                old_lineno += 1
            else:
                assert line[0] == '+'
                result_html += '<tr><td class=emptylineno><td class=lineno>{lineno}{code}</tr>\n'.format(
                    lineno=new_lineno, code=self.prettify_code(i, 'add'))
                new_lineno += 1
        return result_html

    @staticmethod
    def parse(lines):
        """Parses diff lines, and creates a sequence of DiffHunk instances.

        Finds a hunk header, creates a sequence of DiffHunk instances, and
        returns a tuple of the DiffHunk list and unconsumed lines. If a hunk
        header isn't found, ValueError is raised.
        """
        old_start = None
        new_start = None
        context = None
        hunk_lines = None
        hunks = []
        hunk_header_re = r'^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@(.*)?'
        found_hunk_header = False
        for i, line in enumerate(lines):
            if not found_hunk_header:
                match = re.match(hunk_header_re, line)
                if match:
                    found_hunk_header = True
                    old_start = int(match.group(1))
                    new_start = int(match.group(2))
                    context = match.group(3)
                    hunk_lines = []
                continue
            if line.startswith((' ', '-', '+')):
                hunk_lines.append(line)
                continue
            hunks.append(DiffHunk(old_start, new_start, context, hunk_lines))
            match = re.match(hunk_header_re, line)
            if not match:
                return (hunks, lines[i:])
            old_start = int(match.group(1))
            new_start = int(match.group(2))
            context = match.group(3)
            hunk_lines = []
        if found_hunk_header:
            hunks.append(DiffHunk(old_start, new_start, context, hunk_lines))
        else:
            raise ValueError('Found no hunks')
        return (hunks, [])


class BinaryHunk(object):
    """Represents a binary hunk.

    A binary diff for a single file contains two binary hunks. An
    instance of this class represents a single binary hunk.
    """

    def __init__(self, bin_type, size, bin_data):
        assert bin_type in ('literal', 'delta')
        self._type = bin_type
        self._size = size
        self._compressed_data = bin_data

    def prettify(self, mime_type, klass):
        result_html = (
            '<tr><td class=emptylineno><td class=emptylineno>'
            '<td class="{klass} strong binary">Binary {type}; {size}'
            ' Bytes<br>\n').format(
                klass=klass, type=self._type, size=self._size)
        if self._type == 'delta':
            # Because we can assume the input diff is always produced by git, we
            # can obtain the original blob, apply the delta, and render both of
            # the original blob and the patched blob. However, we're not sure
            # how much it is worth to do.
            #
            # For 'delta' format, see patch_delta() in patch-delta.c.
            # https://github.com/git/git/blob/master/patch-delta.c
            return result_html + 'We don\'t support rendering a delta binary hunk.'
        if mime_type.startswith('image/'):
            return result_html + '<img src="data:{type};base64,{data}">'.format(
                type=mime_type,
                data=base64.b64encode(zlib.decompress(self._compressed_data)))
        return result_html + 'We don\'t support rendering {} binary.'.format(
            mime_type)

    @staticmethod
    def parse(lines):
        """Creates a BinaryHunk instance starting with lines[0].

        Returns a tuple of the BinaryHunk instance and unconsumed lines.
        """
        match = re.match(r'(literal|delta) (\d+)', lines[0])
        if not match:
            raise ValueError('No "literal <size>" or "delta <size>".')
        bin_type = match.group(1)
        size = int(match.group(2))
        bin_data = b''

        lines = lines[1:]
        for i, line in enumerate(lines):
            if len(line) == 0:
                return (BinaryHunk(bin_type, size, bin_data), lines[i + 1:])
            line_length_letter = line[0]
            # Map a letter to a number.
            #   A-Z -> 1-26
            #   a-z -> 27-52
            line_length = 1 + ord(line_length_letter) - ord('A')
            if line_length_letter >= 'a':
                line_length = 27 + ord(line_length_letter) - ord('a')
            if line_length * 5 > (len(line) - 1) * 4:
                raise ValueError('Base85 length mismatch: length by the first '
                                 'letter:{}, actual:{}, line:"{}"'.format(
                                     line_length * 5, (len(line) - 1) * 4,
                                     line))
            bin_data += base64.b85decode(line[1:].encode('utf8'))
        raise ValueError('No blank line terminating a binary hunk.')
