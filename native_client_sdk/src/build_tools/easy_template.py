#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import copy
import cStringIO
import os
import re
import sys


STATEMENT_RE = "\[\[(.*?)\]\]"  # [[...]]
EXPR_RE = "\{\{(.*?)\}\}"  # {{...}}

def TemplateToPython(template, statement_re, expr_re):
  output = cStringIO.StringIO()
  indent_re = re.compile(r'\s*')
  indent_string = ''
  for line in template.splitlines(1):  # 1 => keep line ends
    m = statement_re.match(line)
    if m:
      statement = m.group(1)
      indent_string = indent_re.match(statement).group()
      if statement.rstrip()[-1:] == ':':
        indent_string += '  '
      output.write(statement + '\n')
    else:
      line_ending = ''
      while line and line[-1] in '\\"\n\r':
        line_ending = line[-1] + line_ending
        line = line[:-1]

      ms = list(expr_re.finditer(line))
      if ms:
        # Only replace % with %% outside of the expr matches.
        new_line = ''
        start = 0
        for m in ms:
          new_line += line[start:m.start()].replace('%', '%%')
          new_line += line[m.start():m.end()]
          start = m.end()
        new_line += line[start:].replace('%', '%%')
        line = new_line

        subst_line = r'r"""%s""" %% (%s,)' % (
            re.sub(expr_re, '%s', line),
            ', '.join(re.findall(expr_re, line)))
      else:
        subst_line = r'r"""%s"""' % line

      out_string = r'%s__outfile__.write(%s + %s)' % (
          indent_string,
          subst_line,
          repr(line_ending))
      output.write(out_string + '\n')

  return output.getvalue()


def RunTemplate(srcfile, dstfile, template_dict, statement_re=None,
                expr_re=None):
  statement_re = statement_re or re.compile(STATEMENT_RE)
  expr_re = expr_re or re.compile(EXPR_RE)
  script = TemplateToPython(srcfile.read(), statement_re, expr_re)
  template_dict = copy.copy(template_dict)
  template_dict['__outfile__'] = dstfile
  exec script in template_dict


def RunTemplateFile(srcpath, dstpath, template_dict, statement_re=None,
                    expr_re=None):
  with open(srcpath) as srcfile:
    with open(dstpath, 'w') as dstfile:
      RunTemplate(srcfile, dstfile, template_dict, statement_re, expr_re)


def RunTemplateFileIfChanged(srcpath, dstpath, replace):
  dststr = cStringIO.StringIO()
  with open(srcpath) as srcfile:
    RunTemplate(srcfile, dststr, replace)

  if os.path.exists(dstpath):
    with open(dstpath) as dstfile:
      if dstfile.read() == dststr.getvalue():
        return

  with open(dstpath, 'w') as dstfile:
    dstfile.write(dststr.getvalue())


def RunTemplateString(src, template_dict, statement_re=None, expr_re=None):
  srcstr = cStringIO.StringIO(src)
  dststr = cStringIO.StringIO()
  RunTemplate(srcstr, dststr, template_dict, statement_re, expr_re)
  return dststr.getvalue()


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('template')
  options = parser.parse_args(args)

  with open(options.template) as f:
    print TemplateToPython(
        f.read(), re.compile(STATEMENT_RE), re.compile(EXPR_RE))

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
