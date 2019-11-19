#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Builds the complete main.html file from the basic components.
"""

from HTMLParser import HTMLParser
import argparse
import os
import re
import sys


def error(msg):
  print 'Error: %s' % msg
  sys.exit(1)


class HtmlChecker(HTMLParser):
  def __init__(self):
    HTMLParser.__init__(self)
    self.ids = set()

  def handle_starttag(self, tag, attrs):
    for (name, value) in attrs:
      if name == 'id':
        if value in self.ids:
          error('Duplicate id: %s' % value)
        self.ids.add(value)


class GenerateWebappHtml:
  def __init__(self, template_files, js_files, instrumented_js_files,
               template_rel_dir):

    self.js_files = js_files
    self.instrumented_js_files = instrumented_js_files
    self.template_rel_dir = template_rel_dir

    self.templates_expected = set()
    for template in template_files:
      self.templates_expected.add(os.path.basename(template))

    self.templates_found = set()

  def includeJavascript(self, output):
    for js_file in sorted([os.path.basename(x) for x in self.js_files]):
      output.write('    <script src="' + js_file + '"></script>\n')

    for js_path in sorted(self.instrumented_js_files):
      js_file = os.path.basename(js_path)
      output.write('    <script src="' + js_file + '" data-cover></script>\n')

  def verifyTemplateList(self):
    """Verify that all the expected templates were found."""
    if self.templates_expected > self.templates_found:
      extra = self.templates_expected - self.templates_found
      print 'Extra templates specified:', extra
      return False
    return True

  def validateTemplate(self, template_path):
    template = os.path.basename(template_path)
    if template in self.templates_expected:
      self.templates_found.add(template)
      return True
    return False

  def processTemplate(self, output, template_file, indent):
    with open(template_file, 'r') as input_template:
      first_line = True
      skip_header_comment = False

      for line in input_template:
        # If the first line is the start of a copyright notice, then
        # skip over the entire comment.
        # This will remove the copyright info from the included files,
        # but leave the one on the main template.
        if first_line and re.match(r'<!--', line):
          skip_header_comment = True
        first_line = False
        if skip_header_comment:
          if re.search(r'-->', line):
            skip_header_comment = False
          continue

        m = re.match(
            r'^(\s*)<meta-include src="(.+)"\s*/>\s*$',
            line)
        if m:
          prefix = m.group(1)
          template_name = m.group(2)
          template_path = os.path.join(self.template_rel_dir, template_name)
          if not self.validateTemplate(template_path):
            error('Found template not in list of expected templates: %s' %
                  template_name)
          self.processTemplate(output, template_path, indent + len(prefix))
          continue

        m = re.match(r'^\s*<meta-include type="javascript"\s*/>\s*$', line)
        if m:
          self.includeJavascript(output)
          continue

        if line.strip() == '':
          output.write('\n')
        else:
          output.write((' ' * indent) + line)


def parseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '--js',
    nargs='+',
    default={},
    help='The Javascript files to include in HTML <head>')
  parser.add_argument(
    '--js-list-file',
    help='The name of a file containing a list of files, one per line, '
         'identifying the Javascript to include in HTML <head>. This is an '
         'alternate to specifying the files directly via the "--js" option. '
         'The files listed in this file are appended to the files passed via '
         'the "--js" option, if any.')
  parser.add_argument(
    '--templates',
    nargs='*',
    default=[],
    help='The html template files used by input-template')
  parser.add_argument(
    '--exclude-js',
    nargs='*',
    default=[],
    help='The Javascript files to exclude from <--js> and <--instrumentedjs>')
  parser.add_argument(
    '--instrument-js',
    nargs='*',
    default=[],
    help='Javascript to include and instrument for code coverage')
  parser.add_argument(
    '--template-dir',
    default = ".",
    help='Directory template references in html are relative to')
  parser.add_argument('output_file')
  parser.add_argument('input_template')
  return parser.parse_args(sys.argv[1:])


def main():
  args = parseArgs()

  out_file = args.output_file
  js_files = set(args.js)

  # Load the files from the --js-list-file.
  js_list_file = args.js_list_file
  if js_list_file:
    js_files = js_files.union(set(line.rstrip() for line in open(js_list_file)))

  js_files = js_files - set(args.exclude_js)
  instrumented_js_files = set(args.instrument_js) - set(args.exclude_js)

  # Create the output directory if it does not exist.
  out_directory = os.path.dirname(out_file)
  if out_directory and not os.path.exists(out_directory):
    os.makedirs(out_directory)

  # Generate the main HTML file from the templates.
  with open(out_file, 'w') as output:
    gen = GenerateWebappHtml(args.templates, js_files, instrumented_js_files,
                             args.template_dir)
    gen.processTemplate(output, args.input_template, 0)

    # Verify that all the expected templates were found.
    if not gen.verifyTemplateList():
      error('Extra templates specified')

  # Verify that the generated HTML file is valid.
  with open(out_file, 'r') as input_html:
    parser = HtmlChecker()
    parser.feed(input_html.read())


if __name__ == '__main__':
  sys.exit(main())
