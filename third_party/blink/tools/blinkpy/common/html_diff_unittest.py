# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.html_diff import HtmlDiffGenerator, html_diff


class TestHtmlDiff(unittest.TestCase):
    def test_html_diff(self):
        self.assertEqual(
            html_diff('one\ntoo\nthree\n', 'one\ntwo\nthree\n'),
            ('<html>\n'
             '<head>\n'
             '<style>\n'
             'table { white-space: pre-wrap; font-family: monospace; border-collapse: collapse; }\n'
             'th { color: #444; background: #eed; text-align: right; vertical-align: baseline; padding: 1px 4px 1px 4px; }\n'
             '.del { background: #faa; }\n'
             '.add { background: #afa; }\n'
             '</style>\n'
             '</head>\n'
             '<body><table>'
             '<tr><th>1<th>1<td>one\n</tr>'
             '<tr><th>2<th><td class="del">too\n</tr>'
             '<tr><th><th>2<td class="add">two\n</tr>'
             '<tr><th>3<th>3<td>three\n</tr>'
             '</table></body>\n'
             '</html>\n'))

    def test_html_diff_same(self):
        self.assertEqual(
            HtmlDiffGenerator().generate_tbody(['one line\n'], ['one line\n']),
            '<tr><th>1<th>1<td>one line\n</tr>')
        self.assertEqual(
            HtmlDiffGenerator().generate_tbody(['<script>\n'], ['<script>\n']),
            '<tr><th>1<th>1<td>&lt;script&gt;\n</tr>')

    def test_html_diff_delete(self):
        self.assertEqual(
            HtmlDiffGenerator().generate_tbody(['one line\n'], []),
            '<tr><th>1<th><td class="del">one line\n</tr>')
        self.assertEqual(HtmlDiffGenerator().generate_tbody(['</pre>\n'], []),
                         '<tr><th>1<th><td class="del">&lt;/pre&gt;\n</tr>')

    def test_html_diff_insert(self):
        self.assertEqual(
            HtmlDiffGenerator().generate_tbody([], ['one line\n']),
            '<tr><th><th>1<td class="add">one line\n</tr>')
        self.assertEqual(HtmlDiffGenerator().generate_tbody([], ['<!--\n']),
                         '<tr><th><th>1<td class="add">&lt;!--\n</tr>')

    def test_html_diff_ending_newline(self):
        self.assertEqual(
            HtmlDiffGenerator().generate_tbody(['one line'], ['one line\n']),
            '<tr><th>1<th><td class="del">one line</tr><tr><th><th>1<td class="add">one line\n</tr>'
        )

    def test_html_diff_replace_multiple_lines(self):
        a_lines = [
            '1. Beautiful is better than ugly.\n',
            '2. Explicit is better than implicit.\n',
            '3. Simple is better than complex.\n',
            '4. Complex is better than complicated.\n',
        ]
        b_lines = [
            '1. Beautiful is better than ugly.\n',
            '3.   Simple is better than complex.\n',
            '4. Complicated is better than complex.\n',
            '5. Flat is better than nested.\n',
        ]
        self.assertEqual(HtmlDiffGenerator().generate_tbody(
            a_lines, b_lines
        ), ('<tr><th>1<th>1<td>1. Beautiful is better than ugly.\n</tr>'
            '<tr><th>2<th><td class="del">2. Explicit is better than implicit.\n</tr>'
            '<tr><th>3<th><td class="del">3. Simple is better than complex.\n</tr>'
            '<tr><th>4<th><td class="del">4. Complex is better than complicated.\n</tr>'
            '<tr><th><th>2<td class="add">3.   Simple is better than complex.\n</tr>'
            '<tr><th><th>3<td class="add">4. Complicated is better than complex.\n</tr>'
            '<tr><th><th>4<td class="add">5. Flat is better than nested.\n</tr>'
            ))

    def test_html_diff_context(self):
        a_lines = [
            'line1\n',
            'line2\n',
            'line3\n',
            'line4\n',
            'line5\n',
            'line6\n',
            'line7\n',
            'line8\n',
            'line9a\n',
            'line10\n',
            'line11\n',
            'line12\n',
            'line13\n',
            'line14\n',
            'line15a\n',
            'line16\n',
            'line17\n',
            'line18\n',
            'line19\n',
            'line20\n',
            'line21\n',
            'line22\n',
            'line23\n',
        ]
        b_lines = [
            'line1\n',
            'line2\n',
            'line3\n',
            'line4\n',
            'line5\n',
            'line6\n',
            'line7\n',
            'line8\n',
            'line9b\n',
            'line10\n',
            'line11\n',
            'line12\n',
            'line13\n',
            'line14\n',
            'line15b\n',
            'line16\n',
            'line17\n',
            'line18\n',
            'line19\n',
            'line20\n',
            'line21\n',
            'line22\n',
            'line23\n',
        ]
        self.assertEqual(HtmlDiffGenerator().generate_tbody(a_lines, b_lines),
                         ('<tr><td colspan=3>\n\n</tr>'
                          '<tr><th>6<th>6<td>line6\n</tr>'
                          '<tr><th>7<th>7<td>line7\n</tr>'
                          '<tr><th>8<th>8<td>line8\n</tr>'
                          '<tr><th>9<th><td class="del">line9a\n</tr>'
                          '<tr><th><th>9<td class="add">line9b\n</tr>'
                          '<tr><th>10<th>10<td>line10\n</tr>'
                          '<tr><th>11<th>11<td>line11\n</tr>'
                          '<tr><th>12<th>12<td>line12\n</tr>'
                          '<tr><th>13<th>13<td>line13\n</tr>'
                          '<tr><th>14<th>14<td>line14\n</tr>'
                          '<tr><th>15<th><td class="del">line15a\n</tr>'
                          '<tr><th><th>15<td class="add">line15b\n</tr>'
                          '<tr><th>16<th>16<td>line16\n</tr>'
                          '<tr><th>17<th>17<td>line17\n</tr>'
                          '<tr><th>18<th>18<td>line18\n</tr>'
                          '<tr><td colspan=3>\n\n</tr>'))

    def test_html_diff_context_at_edge(self):
        a_lines = [
            'line1\n',
            'line2\n',
            'line3\n',
            'line4\n',
            'line5\n',
            'line6\n',
            'line7\n',
            'line8\n',
        ]
        b_lines = [
            'line0\n',
            'line1\n',
            'line2\n',
            'line3\n',
            'line4\n',
            'line5\n',
            'line6\n',
            'line7\n',
            'line8\n',
            'line9\n',
        ]
        self.assertEqual(HtmlDiffGenerator().generate_tbody(a_lines, b_lines),
                         ('<tr><th><th>1<td class="add">line0\n</tr>'
                          '<tr><th>1<th>2<td>line1\n</tr>'
                          '<tr><th>2<th>3<td>line2\n</tr>'
                          '<tr><th>3<th>4<td>line3\n</tr>'
                          '<tr><td colspan=3>\n\n</tr>'
                          '<tr><th>6<th>7<td>line6\n</tr>'
                          '<tr><th>7<th>8<td>line7\n</tr>'
                          '<tr><th>8<th>9<td>line8\n</tr>'
                          '<tr><th><th>10<td class="add">line9\n</tr>'))
