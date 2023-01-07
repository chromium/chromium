#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Reads, parses, and (optionally) writes as HTML the contents of Markdown
files passed as arguments. Intended for rendering network stack documentation
stored as Markdown in the source tree to a human-readable format."""


import argparse
import os.path
import sys


def nth_parent_directory(path, n):
  for i in range(n):
    path = os.path.dirname(path)
  return path


# Go up the directory tree from this script and add src/third_party to sys.path
# so "import markdown" can find it in src/third_party/markdown.
SCRIPT_PATH = os.path.abspath(__file__)
SRC_PATH = nth_parent_directory(SCRIPT_PATH, 4)
THIRD_PARTY_PATH = os.path.join(SRC_PATH, 'third_party')
sys.path.insert(0, THIRD_PARTY_PATH)
import markdown


def ReadFile(filename):
  with open(filename, 'r') as file:
    return file.read()


def WriteFile(filename, contents):
  dir = os.path.dirname(filename)
  if not os.path.isdir(dir):
    os.mkdir(dir)
  with open(filename, 'w') as file:
    file.write(contents)


TEMPLATE = """
<html>
  <head>
    <title>{title}</title>
  </head>
  <body>
    {body}
  </body>
</html>"""


def FormatPage(markdown_html, title):
  # TODO(juliatuttle): Add a navigation list / table of contents of available
  # Markdown files, perhaps?
  return TEMPLATE.format(title=title, body=markdown_html)


def ProcessDocs(input_filenames, input_pathname, output_pathname,
                extensions=None):
  """Processes a list of Markdown documentation files.

  If input_pathname and output_pathname are specified, outputs HTML files
  into the corresponding subdirectories of output_pathname. If one or both is
  not specified, simply ensures the files exist and contain valid Markdown.

  Args:
      input_filenames: A list of filenames (absolute, or relative to $PWD) of
          Markdown files to parse and possibly render.
      input_pathname: The base directory of the input files. (Needed so they
          can be placed in the same relative path in the output path.)
      output_pathname: The output directory into which rendered Markdown files
          go, using that relative path.
      extensions: a list of Markdown.extensions to apply if any.

  Returns:
      nothing

  Raises:
      IOError: if any of the file operations fail (e.g. input_filenames
          contains a non-existent file).
  """

  outputting = (input_pathname is not None) and (output_pathname is not None)

  if extensions:
    markdown_parser = markdown.Markdown(extensions)
  else:
    markdown_parser = markdown.Markdown()

  for input_filename in input_filenames:
    markdown_text = ReadFile(input_filename)
    markdown_html = markdown_parser.reset().convert(markdown_text)
    if not outputting:
      continue

    full_html = FormatPage(markdown_html, title=input_filename)
    rel_filename = os.path.relpath(input_filename, start=input_pathname)
    output_filename = os.path.join(output_pathname, rel_filename) + '.html'
    WriteFile(output_filename, full_html)


def main():
  parser = argparse.ArgumentParser(
      description='Parse and render Markdown documentation')
  parser.add_argument('--input_path', default=None,
      help="Input path for Markdown; required only if output_path set")
  parser.add_argument('--output_path', default=None,
      help="Output path for rendered HTML; if unspecified, won't output")
  parser.add_argument('filenames', nargs=argparse.REMAINDER)
  args = parser.parse_args()

  extensions = ['markdown.extensions.def_list']
  ProcessDocs(args.filenames, args.input_path, args.output_path, extensions)

  return 0


if __name__ == '__main__':
  sys.exit(main())
